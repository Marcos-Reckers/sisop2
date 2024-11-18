#include "serverClass.h"

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

            std::thread sync_thread(&FileInfo::monitor_sync_dir, "users/sync_dir_" + username_str, client_fd);
            sync_thread.detach();
        }
    }
}

void Server::get_sync_dir(int client_fd)
{
    std::string username = getUsername(client_fd);
    std::string dir_path = "users/sync_dir_" + username;
    FileInfo::create_dir(dir_path);
}

void Server::handle_sync(int sock)
{
    string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    string path = exec_path + "sync_dir_" + getUsername(sock);
    FileInfo::monitor_sync_dir(path, sock);
}

// Método que adiciona o cliente ao map
void Server::addClient(int client_fd, std::string username)
{
    clients[client_fd] = username;
    // client_sockets.insert(username, client_fd);
}

void Server::removeClient(int client_fd)
{
    clients.erase(client_fd);
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
                get_sync_dir(client_sock);
                //sync_client_dir(client_sock);
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

    FileInfo::delete_file(path, client_sock);
    
    for (const auto& client : clients) {
        if (client.second == username) {
            FileInfo::send_cmd("delete", client.first);
            FileInfo::send_file_name(file_name_buffer, client.first);
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
    
    string file_name = FileInfo::receive_file(directory, client_sock);

    string new_path = directory + file_name;
}

void Server::handle_list_request(int client_sock)
{
    std::string username = getUsername(client_sock);
    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    std::string path = exec_path + "/users/sync_dir_" + username;

    cout << "Listando arquivos do diretório: " << path << endl;

    std::vector<FileInfo> files = FileInfo::list_files(path);
    FileInfo::send_list_files(files, client_sock);
}

void Server::close_connection(int client_sock)
{
    send(client_sock, "exit", 4, 0);
}
