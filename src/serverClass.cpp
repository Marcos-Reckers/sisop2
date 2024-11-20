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

        if (client_fd >= 0)
        {
            char username[256] = {0};
            recv(client_fd, username, sizeof(username), 0);
            std::string username_str(username);

            // Se o usuário ainda não tem um semaforo, cria um
            if (active.find(username_str) == active.end())
            {
                active[username_str] = std::make_unique<sem_t>();
                sem_init(active[username_str].get(), 0, 2);
            }

            if (sem_trywait(active[username_str].get()) != 0)
            {
                std::cout << "O usuário " << username_str << " já possui duas sessões ativas. Conexão não permitida." << std::endl;
                send(client_fd, "exit", 4, 0);
            }
            else
            {
                send(client_fd, "ok", 2, 0);
            }

            addClient(client_fd, username_str);
            client_threads.emplace_back(&Server::handle_communication, this, client_fd);

            // handle_communication igual do cliente

            std::cout << "Conexão aceita de: " << username_str << std::endl;
        }
        else
        {
            std::cerr << "Erro ao aceitar conexão do cliente." << std::endl;
            continue;
        }
    }
}

void Server::handle_communication(int client_sock)
{
    if (client_sock > 0)
    {
        // Codigo para deixar não bloqeante entre recv e send
        //  ===================================================================
        auto flags = ::fcntl(client_sock, F_GETFL, 0);
        if (flags == -1)
        {
            close(client_sock);
            cout << "Erro ao obter flags do socket." << std::endl;
            return;
        }
        flags |= O_NONBLOCK;
        auto fcntl_result = ::fcntl(client_sock, F_SETFL, flags);
        if (fcntl_result == -1)
        {
            close(client_sock);
            cout << "Erro ao setar flags do socket." << endl;
            return;
        }
        // ===================================================================

        // inicializa as filas
        //  ===================================================================
        Threads::AtomicQueue<std::vector<Packet>> send_queue;
        Threads::AtomicQueue<std::vector<Packet>> received_queue;
        Threads::AtomicQueue<std::vector<Packet>> sync_queue;
        // ===================================================================

        // Cria a pasta do cliente no servidor para sincronização
        // ===================================================================
        create_sync_dir(client_sock);
        // ===================================================================

        // cria as threds
        //  ===================================================================
        //  criathread de sync
        auto client_folder = "sync_dir_" + getUsername(client_sock);
        std::thread sync_thread([client_sock, client_folder, &send_queue, &sync_queue]()
                                { Server::handle_sync(client_sock, client_folder, send_queue, sync_queue); });
        // cria thread de comandos
        std::thread command_thread([client_sock, client_folder, &send_queue, &received_queue]()
                                   { Server::handle_commands(client_sock, client_folder, send_queue, received_queue); });
        // cria thread de monitoramento
        std::thread monitor_thread([client_sock, client_folder, &send_queue]()
                                   { Server::monitor_sync_dir(client_sock, client_folder, send_queue); });
        // ===================================================================

        while (client_sock > 0)
        {
            // consumir do send_queue e enviar para o servidor na sock
            auto maybe_packet = send_queue.consume();
            if (maybe_packet.has_value())
            {
                auto packet = maybe_packet.value();
                for (auto pkt : packet)
                {
                    std::vector<uint8_t> packet_bytes = Packet::packet_to_bytes(pkt);
                    ssize_t sent_bytes = FileInfo::sendAll(client_sock, packet_bytes.data(), packet_bytes.size(), 0);
                    if (sent_bytes < 0)
                    {
                        std::cerr << "Erro ao enviar pacote." << std::endl;
                    }
                    std::cout << "Pacote " << pkt.get_seqn() << " de tamanho: " << sent_bytes << " bytes enviado." << std::endl;
                }
            }

            // pegar da sock e colocar na received_queue ou sync_queue
            std::vector<uint8_t> packet_bytes;

            ssize_t received_bytes = FileInfo::recvAll(client_sock, packet_bytes.data(), packet_bytes.size(), 0);
            if (received_bytes < 0)
            {
                std::cerr << "Erro ao receber pacote." << std::endl;
            }
            else if (received_bytes != 0)
            {
                Packet received_packet = Packet::bytes_to_packet(packet_bytes);
                if (received_packet.get_type() == 1)
                {
                    sync_queue.produce({received_packet});
                }
                else if (received_packet.get_type() == 2)
                {
                    received_queue.produce({received_packet});
                }
                else if (received_packet.get_type() == 3)
                {
                    std::cout << "Conexão encerrada pelo servidor." << std::endl;
                    close(client_sock);
                    client_sock = -1;
                }
                else
                {
                    std::cerr << "Pacote recebido com tipo inválido." << std::endl;
                }
            }
        }

        sync_thread.join();
        command_thread.join();
        monitor_thread.join();

        return;
    }
    else
    {
        std::cerr << "Erro ao aceitar conexão do cliente." << std::endl;
        return;
    }
}

void Server::handle_commands(int client_sock, string folder_name, Threads::AtomicQueue<std::vector<Packet>> &send_queue, Threads::AtomicQueue<std::vector<Packet>> &received_queue)
{
    std::cout << "LIDANDO COM COMMANDS UAUUUUUUUUUUUU" << std::endl;
    string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();

    while (client_sock > 0)
    {
        auto packet = received_queue.consume_blocking();
        if (packet[0].get_type() == 1)
        {
            string cmd = packet[0].get_payload_as_string();
            if (cmd == "upload")
            {
                string file_name = FileInfo::receive_file(received_queue, folder_name);
                std::cout << "Arquivo recebido: " << file_name << std::endl;
            }
            else if (cmd == "download")
            {
                auto download_packet = received_queue.consume_blocking();
                FileInfo file_info = FileInfo::receive_file_info(download_packet);
                string file_name = file_info.get_file_name();
                string file_path = exec_path + "/" + folder_name + "/" + file_name;
                send_queue.produce(FileInfo::create_packet_vector("download_response", file_path));
                std::cout << "Arquivo enviado: " << file_name << std::endl;
            }
            else if (cmd == "delete")
            {
                auto delete_packet = received_queue.consume_blocking();
                FileInfo file_info = FileInfo::receive_file_info(delete_packet);
                string file_name = file_info.get_file_name();
                FileInfo::delete_file(file_name);
                std::cout << "Arquivo deletado: " << file_name << std::endl;
            }
            else if (cmd == "list_server")
            {
                string folder_path = exec_path + "/" + folder_name;
                send_queue.produce(FileInfo::create_packet_vector("list_server_response", folder_path));
            }
            else if (cmd == "exit")
            {
                close(client_sock);
                send_queue.produce(FileInfo::create_packet_vector("exit", ""));
            }
        }
    }
}

void Server::create_sync_dir(int client_fd)
{
    std::string username = getUsername(client_fd);
    std::string dir_path = "users/sync_dir_" + username;
    FileInfo::create_dir(dir_path);
}

void Server::handle_sync(int client_sock, std::string folder_name, Threads::AtomicQueue<std::vector<Packet>> &send_queue, Threads::AtomicQueue<std::vector<Packet>> &sync_queue)
{
    std::cout << "LIDANDO COM SYNC UAUUUUUUUUUUUU" << std::endl;
    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();

    while (client_sock > 0)
    {
        auto packet = sync_queue.consume_blocking();
        if (packet[0].get_type() == 1)
        {
            string cmd = packet[0].get_payload_as_string();
            if (cmd == "upload")
            {
                // le a lista de pacotes e monta o arquivo
                string file_name = FileInfo::receive_file(sync_queue, folder_name);
                std::cout << "Arquivo recebido: " << file_name << std::endl;
            }
            else if (cmd == "download")
            {
                auto sync_packet = sync_queue.consume_blocking();
                FileInfo file_info = FileInfo::receive_file_info(sync_packet);
                string file_name = file_info.get_file_name();
                string file_path = exec_path + "/" + folder_name + "/" + file_name;
                send_queue.produce(FileInfo::create_packet_vector("download_response", file_path));
                std::cout << "Arquivo enviado: " << file_name << std::endl;
            }
            else if (cmd == "delete")
            {
                auto sync_packet = sync_queue.consume_blocking();
                FileInfo file_info = FileInfo::receive_file_info(sync_packet);
                string file_name = file_info.get_file_name();
                FileInfo::delete_file(file_name);
                std::cout << "Arquivo deletado: " << file_name << std::endl;
            }
        }
    }
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

// Metodo para adicionar uma thread de cliente ao vetor de threads
void Server::addClientThread(std::thread &&thread) { client_threads.push_back(std::move(thread)); }

// Getters
int Server::getServerFd() const { return server_fd; }
sockaddr_in &Server::getServerAddr() { return server_addr; }
socklen_t Server::getAddrLen() const { return sizeof(server_addr); }
std::vector<std::thread> &Server::getClientThreads() { return client_threads; }
std::map<int, std::string> &Server::getClients() { return clients; }

void Server::close_connection(int client_sock)
{
    send(client_sock, "exit", 4, 0);
}

void Server::monitor_sync_dir(int client_sock, string folder_name, Threads::AtomicQueue<std::vector<Packet>> &send_queue)
{

    FileInfo::create_dir(folder_name);

    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    std::string sync_dir = exec_path + "/" + folder_name;

    int fd = inotify_init();
    if (fd < 0)
    {
        perror("inotify_init");
        return;
    }

    int wd = inotify_add_watch(fd, sync_dir.c_str(), IN_CLOSE_WRITE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
    if (wd < 0)
    {
        perror("inotify_add_watch");
        close(fd);
        return;
    }

    const size_t buf_size = 1024 * (sizeof(struct inotify_event) + NAME_MAX + 1);
    char *buffer = new char[buf_size];

    while (client_sock > 0)
    {
        int length = read(fd, buffer, buf_size);
        if (length < 0)
        {
            perror("read");
            break;
        }

        int i = 0;
        while (i < length)
        {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];
            if (event->len)
            {
                if (event->mask & IN_CLOSE_WRITE || event->mask & IN_MOVED_TO)
                {
                    string file_name = event->name;
                    cout << "Arquivo pronto para envio: " << file_name << endl;
                    string file_path = sync_dir + "/" + file_name;
                    send_queue.produce(FileInfo::create_packet_vector("upload", file_path));
                }
                if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM)
                {
                    string file_name = event->name;
                    cout << "Arquivo deletado: " << file_name << endl;
                    send_queue.produce(FileInfo::create_packet_vector("delete", file_name));
                }
            }
            i += sizeof(struct inotify_event) + event->len;
        }
    }

    delete[] buffer;
    inotify_rm_watch(fd, wd);
    close(fd);
}

// void Server::handleRequest(int client_sock)
// {
//     std::cout << "Entrou em handleRequest, com: " << getUsername(client_sock) << std::endl;

//     while (true)
//     {
//         ssize_t total_size = Packet::packet_base_size() + MAX_PAYLOAD_SIZE;
//         std::vector<uint8_t> packet_buffer(total_size);
//         ssize_t received_bytes = recv(client_sock, packet_buffer.data(), packet_buffer.size(), 0);

//         if (received_bytes < 0)
//         {
//             std::cerr << "Erro ao receber o pacote." << std::endl;
//             return;
//         }

//         if (received_bytes == 0)
//         {

//             std::string username = getUsername(client_sock);

//             // Manda um sinal para o semaforo para liberar um novo dispositivo
//             if (active.find(username) != active.end())
//             {
//                 sem_post(active[username].get());
//             }

//             std::cout << "Conexão do cliente " << username << " encerrada." << std::endl;
//             removeClient(client_sock);
//             close(client_sock);

//             return;
//         }

//         Packet pkt = Packet::bytes_to_packet(packet_buffer);
//         pkt.print();

//         if (pkt.get_type() == 1)
//         {
//             std::vector<char> cmd_vec = pkt.get_payload();
//             std::string cmd(cmd_vec.begin(), cmd_vec.end());
//             std::cout << "Comando recebido: " << cmd << std::endl;

//             if (cmd.substr(0, 6) == "upload")
//             {
//                 handle_upload_request(client_sock);
//             }
//             if (cmd.substr(0, 8) == "download")
//             {
//                 ;
//                 handle_download_request(client_sock);
//             }
//             if (cmd.substr(0, 6) == "delete")
//             {
//                 handle_delete_request(client_sock);
//             }
//             if (cmd.substr(0, 11) == "list_server")
//             {
//                 handle_list_request(client_sock);
//             }
//             if (cmd.substr(0, 12) == "get_sync_dir")
//             {
//                 create_sync_dir(client_sock);
//                 cout << "get_sync_dir criado e sincronização implementada" << endl;
//             }
//             if (cmd.substr(0, 4) == "exit")
//             {
//                 close_connection(client_sock);
//             }
//         }
//         else
//         {
//             std::cerr << "Pacote recebido não é do tipo 1." << std::endl;
//         }
//     }
// }
// void Server::handle_delete_request(int client_sock)
// {
//     char file_name_buffer[256] = {0};
//     std::string username = getUsername(client_sock);
//     ssize_t received_bytes = recv(client_sock, file_name_buffer, sizeof(file_name_buffer), 0);
//     if (received_bytes <= 0)
//     {
//         std::cerr << "Erro ao receber o nome do arquivo." << std::endl;
//         return;
//     }

//     std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
//     std::string path = exec_path + "/users/sync_dir_" + username + "/" + file_name_buffer;

//     FileInfo::delete_file(path);
// }

// void Server::handle_download_request(int client_sock)
// {
//     char file_name_buffer[256] = {0};
//     std::string username = getUsername(client_sock);
//     ssize_t received_bytes = recv(client_sock, file_name_buffer, sizeof(file_name_buffer), 0);

//     std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
//     std::string file_path = exec_path + "/users/sync_dir_" + username + "/" + file_name_buffer;
//     std::cout << "Arquivo a ser enviado: " << file_path << std::endl;

//     if (received_bytes <= 0)
//     {
//         std::cerr << "Erro ao receber o nome do arquivo." << std::endl;
//         return;
//     }

//     if (!std::filesystem::exists(file_path))
//     {
//         std::cerr << "Arquivo não encontrado." << std::endl;
//         std::string error_msg = "ERROR: Arquivo não encontrado.";
//         Packet error_packet = Packet::create_packet_cmd(error_msg);
//         std::vector<uint8_t> error_packet_bytes = Packet::packet_to_bytes(error_packet);
//         send(client_sock, error_packet_bytes.data(), error_packet_bytes.size(), 0);
//         return;
//     }

//     FileInfo::send_file(file_path, client_sock);
// }

// void Server::handle_list_request(int client_sock)
// {
//     std::string username = getUsername(client_sock);
//     std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
//     std::string path = exec_path + "/users/sync_dir_" + username;

//     cout << "Listando arquivos do diretório: " << path << endl;

//     std::vector<FileInfo> files = FileInfo::list_files(path);
//     FileInfo::send_list_files(files, client_sock);
// }
