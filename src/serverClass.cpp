#include "serverClass.h"
#include <mutex>
#include <chrono>
#include <thread>

// Construtor da classe que recebe a porta como argumento
Server::Server(int port) : server_fd(-1), port(port)
{
    memset(&server_addr, 0, sizeof(server_addr));
}

// Método para iniciar o servidor
bool Server::start()
{
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
    {
        std::cerr << "Erro ao criar o socket." << std::endl;
        return false;
    }

    // Configuração do endereço do servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Escuta em qualquer interface
    server_addr.sin_port = htons(port);

    // Bind do socket à porta
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        std::cerr << "Erro ao fazer o bind na porta " << port << "." << std::endl;
        close(server_fd);
        return false;
    }

    // Coloca o servidor em modo de escuta
    if (listen(server_fd, 10) < 0)
    { // 10 clientes na fila de espera
        std::cerr << "Erro ao colocar o servidor em modo de escuta." << std::endl;
        close(server_fd);
        return false;
    }

    std::cout << "Servidor iniciado e aguardando conexões na porta " << port << "." << std::endl;
    return true;
}

// Método para aceitar conexões de clientes em novas threads
void Server::acceptClients()
{
    while (true)
    {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        // Aceita a conexão do cliente
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd < 0)
        {
            std::cerr << "Erro ao aceitar conexão do cliente." << std::endl;
            continue;
        }
        else
        {
            char username[256] = {0};

            recv(client_fd, username, sizeof(username), 0);
            std::string username_str(username);

            // Se o usuário ainda não tem um semaforo, cria um
            if (active.find(username_str) == active.end())
            {
                active[username_str] = std::make_unique<sem_t>();
                sem_init(active[username_str].get(), 0, 2); // Semaforo com 2 slots
            }

            if (sem_trywait(active[username_str].get()) != 0)
            {
                std::cout << "O usuário " << username_str << " já possui duas sessões ativas. Conexão não permitida." << std::endl;
                send(client_fd, "exit", 4, 0);
            }else{
                send(client_fd, "ok", 2, 0);
            }

            addClient(client_fd, username_str);
            client_threads.emplace_back(&Server::handleRequest, this, client_fd);

            std::cout << "Conexão aceita de: " << username_str << std::endl;

            // // TODO: FIZ NOVO
            // //  Inicia a thread de sincronização
            // std::thread sync_thread(&Server::sync_files, this, client_fd);
            // sync_thread.detach(); // Desanexa a thread para que ela rode em segundo plano
        }
    }
}


void Server::handle_sync(int client_sock)
{
    std::string username = getUsername(client_sock);
    std::string folder = "users/sync_dir_" + username;
    monitor_sync_dir(folder, client_sock);
}

void Server::monitor_sync_dir(std::string folder, int origin_sock)
{
    FileInfo::create_dir(folder);

    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    std::string sync_dir = exec_path + "/" + folder;

    int fd = inotify_init();
    if (fd < 0) {
        perror("inotify_init");
        return;
    }

    int wd = inotify_add_watch(fd, sync_dir.c_str(), IN_CLOSE_WRITE | IN_DELETE | IN_MOVED_FROM);
    if (wd < 0) {
        perror("inotify_add_watch");
        close(fd);
        return;
    }

    const size_t buf_size = 1024 * (sizeof(struct inotify_event) + NAME_MAX + 1);
    char *buffer = new char[buf_size];

    while (true) {
        int length = read(fd, buffer, buf_size);
        if (length < 0) {
            perror("read");
            break;
        }

        int i = 0;
        while (i < length) {
            struct inotify_event *event = (struct inotify_event *) &buffer[i];

            if (event->len) {
                string file_name = event->name;
                std::string username = getUsername(origin_sock);

                if (event->mask & IN_CLOSE_WRITE) {
                    cout << "Arquivo modificado: " << file_name << endl;
                    broadcast_to_user(username, file_name, "upload", origin_sock);
                }
                if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM) {
                    cout << "Arquivo deletado: " << file_name << endl;
                    broadcast_to_user(username, file_name, "delete", origin_sock);
                }
            }
            i += sizeof(struct inotify_event) + event->len;
        }
    }

    delete[] buffer;
    inotify_rm_watch(fd, wd);
    close(fd);
}

// Método que adiciona o cliente ao map
void Server::addClient(int client_fd, std::string username)
{
    clients[client_fd] = username;
    user_sessions[username].push_back(client_fd);
}

void Server::removeClient(int client_fd)
{
    std::string username = clients[client_fd];
    auto& sessions = user_sessions[username];
    sessions.erase(std::remove(sessions.begin(), sessions.end(), client_fd), sessions.end());
    if (sessions.empty()) {
        user_sessions.erase(username);
    }
    clients.erase(client_fd);
}

void Server::broadcast_to_user(std::string username, std::string file_name, std::string command, int origin_sock)
{
    if (user_sessions.find(username) == user_sessions.end()) {
        return;
    }

    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    std::string sync_dir = exec_path + "/users/sync_dir_" + username;

    std::lock_guard<std::mutex> lock(broadcast_mutex);
    static std::map<std::string, std::chrono::steady_clock::time_point> last_broadcast;
    auto now = std::chrono::steady_clock::now();
    
    // Verifica se já fez broadcast deste arquivo recentemente
    std::string key = username + "_" + file_name + "_" + command;
    if (last_broadcast.find(key) != last_broadcast.end()) {
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_broadcast[key]).count() < 2) {
            return;
        }
    }
    last_broadcast[key] = now;

    for (int client_sock : user_sessions[username]) {
        if (client_sock != origin_sock) {
            try {
                FileInfo::send_cmd(command, client_sock);
                if (command == "upload") {
                    std::string file_path = sync_dir + "/" + file_name;
                    if (std::filesystem::exists(file_path)) {
                        FileInfo::send_file(file_path, client_sock);
                    }
                } else if (command == "delete") {
                    FileInfo::send_file_name(file_name, client_sock);
                }
                // Pequeno delay para garantir que os pacotes sejam processados na ordem correta
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            } catch (const std::exception& e) {
                std::cerr << "Erro ao enviar para cliente " << client_sock << ": " << e.what() << std::endl;
            }
        }
    }
}

std::string Server::getUsername(int client_fd)
{
    return clients[client_fd];
}

// Método para encerrar o servidor
void Server::stop()
{
    if (server_fd != -1)
    {
        close(server_fd);
        server_fd = -1;
        std::cout << "Servidor encerrado." << std::endl;
    }

    // Aguarda que todas as threads dos clientes terminem
    for (auto &thread : client_threads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }
    std::cout << "Todas as conexões de cliente foram encerradas." << std::endl;
}

void Server::handleRequest(int client_sock)
{
    std::cout << "Entrou em handleRequest, com: " << getUsername(client_sock) << std::endl;

    while (true)
    {
        ssize_t total_size = Packet::packet_base_size() + MAX_PAYLOAD_SIZE;
        std::vector<uint8_t> packet_buffer(total_size);
        ssize_t received_bytes = recv(client_sock, packet_buffer.data(), packet_buffer.size(), 0);

        if (received_bytes < 0)
        {
            std::cerr << "Erro ao receber o pacote." << std::endl;
            return;
        }

        if (received_bytes == 0)
        {

            std::string username = getUsername(client_sock);

            // Manda um sinal para o semaforo para liberar um novo dispositivo
            if (active.find(username) != active.end())
            {
                sem_post(active[username].get());
            }

            std::cout << "Conexão do cliente " << username << " encerrada." << std::endl;
            removeClient(client_sock);
            close(client_sock);

            return;
        }

        Packet pkt = Packet::bytes_to_packet(packet_buffer);
        pkt.print();

        if (pkt.get_type() == 1)
        {
            std::vector<char> cmd_vec = pkt.get_payload();
            std::string cmd(cmd_vec.begin(), cmd_vec.end());
            std::cout << "Comando recebido: " << cmd << std::endl;
            
            if (cmd.substr(0, 6) == "upload")
            {
                handle_upload_request(client_sock);
            }
            if (cmd.substr(0, 8) == "download")
            {;
                handle_download_request(client_sock);
            }
            if (cmd.substr(0, 6) == "delete")
            {
                handle_delete_request(client_sock);
            }
            if (cmd.substr(0, 11) == "list_server")
            {
                handle_list_request(client_sock);
            }
            if (cmd.substr(0, 12) == "get_sync_dir")
            {
                std::thread sync_thread(&Server::handle_sync, this, client_sock);
                sync_thread.detach();
                cout << "get_sync_dir criado e sincronização implementada" << endl;
            }
            if (cmd.substr(0, 4) == "exit")
            {
                close_connection(client_sock);
            }
        }
        else
        {
            std::cerr << "Pacote recebido não é do tipo 1." << std::endl;
        }
    }
}

// Metodo para adicionar uma thread de cliente ao vetor de threads
void Server::addClientThread(std::thread &&thread) { client_threads.push_back(std::move(thread)); }

// Getters
int Server::getServerFd() const { return server_fd; }
sockaddr_in &Server::getServerAddr() { return server_addr; }
socklen_t Server::getAddrLen() const { return sizeof(server_addr); }
std::vector<std::thread> &Server::getClientThreads() { return client_threads; }
std::map<int, std::string> &Server::getClients(){ return clients; }

void Server::handle_delete_request(int client_sock)
{
    char file_name_buffer[256] = {0};
    std::string username = getUsername(client_sock);
    ssize_t received_bytes = recv(client_sock, file_name_buffer, sizeof(file_name_buffer), 0);
    if (received_bytes <= 0)
    {
        std::cerr << "Erro ao receber o nome do arquivo." << std::endl;
        return;
    }

    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    std::string path = exec_path + "/users/sync_dir_" + username + "/" + file_name_buffer;

    if (FileInfo::delete_file(path, client_sock)) {
        // Propaga o delete para outras sessões do mesmo usuário
        for (int other_sock : user_sessions[username]) {
            if (other_sock != client_sock) {
                try {
                    FileInfo::send_cmd("delete", other_sock);
                    FileInfo::send_file_name(file_name_buffer, other_sock);
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                } catch (const std::exception& e) {
                    std::cerr << "Erro ao propagar delete para cliente " << other_sock << ": " << e.what() << std::endl;
                }
            }
        }
    }
}

void Server::handle_download_request(int client_sock)
{
    char file_name_buffer[256] = {0};
    std::string username = getUsername(client_sock);
    ssize_t received_bytes = recv(client_sock, file_name_buffer, sizeof(file_name_buffer), 0);

    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    std::string file_path = exec_path + "/users/sync_dir_" + username + "/" + file_name_buffer;
    std::cout << "Arquivo a ser enviado: " << file_path << std::endl;

    if (received_bytes <= 0)
    {
        std::cerr << "Erro ao receber o nome do arquivo." << std::endl;
        return;
    }

    if (!std::filesystem::exists(file_path))
    {
        std::cerr << "Arquivo não encontrado." << std::endl;
        std::string error_msg = "ERROR: Arquivo não encontrado.";
        Packet error_packet = Packet::create_packet_cmd(error_msg);
        std::vector<uint8_t> error_packet_bytes = Packet::packet_to_bytes(error_packet);
        send(client_sock, error_packet_bytes.data(), error_packet_bytes.size(), 0);
        return;
    }

    FileInfo::send_file(file_path, client_sock);
}

void Server::handle_upload_request(int client_sock)
{
    std::string username = getUsername(client_sock);
    std::string directory = "users/sync_dir_" + username + "/";
    
    std::string received_file = FileInfo::receive_file(directory, client_sock);
    if (!received_file.empty()) {
        std::string file_path = std::filesystem::canonical("/proc/self/exe").parent_path().string() 
                               + "/" + directory + received_file;
                               
        // Primeiro envia o arquivo de volta para o cliente que fez o upload
        if (std::filesystem::exists(file_path)) {
            FileInfo::send_file(file_path, client_sock);
        }
        
        // Depois faz broadcast para todas as outras sessões
        for (int other_sock : user_sessions[username]) {
            if (other_sock != client_sock) {
                try {
                    FileInfo::send_cmd("upload", other_sock);
                    FileInfo::send_file(file_path, other_sock);
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                } catch (const std::exception& e) {
                    std::cerr << "Erro ao enviar para cliente " << other_sock << ": " << e.what() << std::endl;
                }
            }
        }
    }
}

void Server::handle_list_request(int client_sock)
{
    std::string username = getUsername(client_sock);
    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    std::string path = exec_path + "/users/sync_dir_" + username;

    cout << "Listando arquivos do diretório: " << path << endl;
    FileInfo::create_dir("users/sync_dir_" + username);

    std::vector<FileInfo> files = FileInfo::list_files(path);
    if (files.empty()) {
        cout << "Diretório vazio" << endl;
    }
    FileInfo::send_list_files(files, client_sock);
}

void Server::close_connection(int client_sock)
{
    send(client_sock, "exit", 4, 0);
}
