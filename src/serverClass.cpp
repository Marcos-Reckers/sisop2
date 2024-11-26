#include "serverClass.h"
#include <mutex>

std::mutex send_packets_mutex;
std::mutex recive_packets_mutex;

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
                continue;
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

void Server::handle_io(int &client_sock, Threads::AtomicQueue<std::vector<Packet>> &send_queue, Threads::AtomicQueue<std::vector<Packet>> &received_queue, Threads::AtomicQueue<std::vector<Packet>> &sync_queue)
{
    vector<Packet> packets_to_recv_queue;
    vector<Packet> packets_to_sync_queue;

    while (client_sock > 0)
    {
        // consumir do send_queue e enviar para o servidor na sock
        auto maybe_packet = send_queue.consume();
        if (maybe_packet.has_value())
        {
            auto packet = maybe_packet.value();
            if (packet[0].get_type() == 4)
            {
                for (auto client : clients)
                {
                    if (getUsername(client_sock) == client.second && client.first != client_sock)
                    {
                        for (auto pkt : packet)
                        {
                            pkt.set_type(2);
                            std::vector<uint8_t> packet_bytes = Packet::packet_to_bytes(pkt);
                            ssize_t sent_bytes = FileInfo::sendAll(client.first, packet_bytes.data(), packet_bytes.size(), 0);
                            if (sent_bytes < 0)
                            {
                                std::cerr << "Erro ao enviar pacote." << std::endl;
                            }
                            std::cout << "Enviado pacote " << pkt.get_seqn() << "/" << pkt.get_total_packets() << " de tamanho: " << sent_bytes << " via broadcast" << std::endl;
                        }
                    }
                }
                continue;
            }
            else if (packet[0].get_type() == 5)
            {
                for (auto pkt : packet)
                {
                    pkt.set_type(1);
                    std::vector<uint8_t> packet_bytes = Packet::packet_to_bytes(pkt);
                    ssize_t sent_bytes = FileInfo::sendAll(client_sock, packet_bytes.data(), packet_bytes.size(), 0);
                    if (sent_bytes < 0)
                    {
                        std::cerr << "Erro ao enviar pacote." << std::endl;
                    }
                    std::cout << "Enviado pacote " << pkt.get_seqn() << "/" << pkt.get_total_packets() << " de tamanho: " << sent_bytes << " via response" << std::endl;
                }
                continue;
            }
            else if (packet[0].get_type() == 3)
            {
                for (auto pkt : packet)
                {
                    std::vector<uint8_t> packet_bytes = Packet::packet_to_bytes(pkt);
                    ssize_t sent_bytes = FileInfo::sendAll(client_sock, packet_bytes.data(), packet_bytes.size(), 0);
                    if (sent_bytes < 0)
                    {
                        std::cerr << "Erro ao enviar pacote." << std::endl;
                    }
                    std::cout << "Enviado pacote " << pkt.get_seqn() << "/" << pkt.get_total_packets() << " de tamanho: " << sent_bytes << " via response" << std::endl;
                }
                continue;
            } 
            else
            {
                for (auto client : clients)
                {
                    if (getUsername(client_sock) == client.second)
                    {
                        for (auto pkt : packet)
                        {
                            std::vector<uint8_t> packet_bytes = Packet::packet_to_bytes(pkt);
                            ssize_t sent_bytes = FileInfo::sendAll(client.first, packet_bytes.data(), packet_bytes.size(), 0);
                            if (sent_bytes < 0)
                            {
                                std::cerr << "Erro ao enviar pacote." << std::endl;
                            }
                            std::cout << "Enviado pacote " << pkt.get_seqn() << "/" << pkt.get_total_packets() << " de tamanho: " << sent_bytes << std::endl;
                        }
                    }
                }
            }
        }

        ssize_t total_bytes = Packet::packet_header_size() + MAX_PAYLOAD_SIZE;
        std::vector<uint8_t> packet_bytes(total_bytes);
        // sleep(1);

        ssize_t received_bytes = FileInfo::wait_and_receive(client_sock, packet_bytes, total_bytes, std::chrono::milliseconds(100));
        // ssize_t received_bytes = FileInfo::recvAll(client_sock, packet_bytes, total_bytes);
        if (received_bytes == 0)
        {
            std::string username = getUsername(client_sock);

            if (active.find(username) != active.end())
            {
                sem_post(active[username].get());
            }

            std::cout << "Conexão do cliente " << username << " encerrada." << std::endl;
            removeClient(client_sock);
            close(client_sock);
            client_sock = -1;
        }
        else if (received_bytes > 0)
        {
            Packet received_packet = Packet::bytes_to_packet(packet_bytes);
            cout << "Recebeu pacote " << received_packet.get_seqn() << "/" << received_packet.get_total_packets() << " de tamanho: " << received_bytes << endl;

            if (received_packet.get_type() == 1)
            {
                if (received_packet.get_seqn() == received_packet.get_total_packets())
                {
                    packets_to_recv_queue.push_back(received_packet);
                    received_queue.produce(packets_to_recv_queue);
                    packets_to_recv_queue.clear();
                }
                else if (received_packet.get_seqn() < received_packet.get_total_packets())
                {
                    packets_to_recv_queue.push_back(received_packet);
                }
            }
            else if (received_packet.get_type() == 2)
            {
                if (received_packet.get_seqn() == received_packet.get_total_packets())
                {
                    packets_to_sync_queue.push_back(received_packet);
                    sync_queue.produce(packets_to_sync_queue);
                    packets_to_sync_queue.clear();
                }
                else if (received_packet.get_seqn() < received_packet.get_total_packets())
                {
                    packets_to_sync_queue.push_back(received_packet);
                }
            }
            else
            {
                std::cerr << "Pacote recebido com tipo inválido." << std::endl;
                received_packet.print();
            }
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

        std::thread io_thread(
            [this, &client_sock, &send_queue, &received_queue, &sync_queue]()
            {
                this->handle_io(client_sock,
                                send_queue,
                                received_queue,
                                sync_queue);
            });
        // Cria a pasta do cliente no servidor para sincronização
        // ===================================================================
        create_sync_dir(client_sock);
        // ===================================================================

        // cria as threds
        //  ===================================================================
        auto client_folder = "sync_dir_" + getUsername(client_sock);
        // cria thread de comandos
        std::thread command_thread([&client_sock, client_folder, &send_queue, &received_queue]()
                                   { Server::handle_commands(client_sock, client_folder, send_queue, received_queue); });
        //  criathread de sync
        std::thread sync_thread([&client_sock, client_folder, &send_queue, &sync_queue]()
                                { Server::handle_sync(client_sock, client_folder, send_queue, sync_queue); });

        io_thread.join();
        command_thread.join();
        sync_thread.join();

        return;
    }
    else
    {
        std::cerr << "Erro ao aceitar conexão do cliente." << std::endl;
        return;
    }
}

void Server::handle_commands(int &client_sock, string folder_name, Threads::AtomicQueue<std::vector<Packet>> &send_queue, Threads::AtomicQueue<std::vector<Packet>> &received_queue)
{
    std::cout << "A thread para lidar com comandos no servidor está executando." << std::endl;
    string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();

    while (client_sock > 0)
    {
        auto packets = received_queue.consume_blocking();
        packets[0].clean_payload();
        cout << "Comando recebido: " << packets[0].get_payload_as_string() << endl;
        if (packets[0].get_type() == 1)
        {
            string cmd = packets[0].get_payload_as_string();
            if (cmd == "get_sync_dir")
            {
                cout << "Enviando lista de arquivos do servidor" << endl;
            }
            else if (cmd == "upload")
            {
                string file_name = FileInfo::receive_file(packets, folder_name);
                std::cout << "Arquivo recebido: " << file_name << std::endl;
                string file_path = exec_path + "/" + folder_name + "/" + file_name;
                cout << "Arquivo pronto para envio via upload_sync: " << file_path << endl;
                auto pkts = FileInfo::create_packet_vector("upload_sync", file_path);
                send_queue.produce(pkts);
            }
            else if (cmd == "download")
            {
                FileInfo file_info = FileInfo::receive_file_info(packets);
                string file_name = file_info.get_file_name();
                string file_path = exec_path + "/" + folder_name + "/" + file_name;
                auto pkts = FileInfo::create_packet_vector("download_response", file_path);
                send_queue.produce(pkts);
                std::cout << "Arquivo enviado: " << file_name << std::endl;
            }
            else if (cmd == "delete")
            {
                FileInfo file_info = FileInfo::receive_file_info(packets);
                string file_name = file_info.get_file_name();
                string file_path = exec_path + "/" + folder_name + "/" + file_name;
                cout << "Enviando delete_sync: " << file_name << endl;
                auto pkts = FileInfo::create_packet_vector("delete_sync", file_name);
                send_queue.produce(pkts);
                std::cout << "Deletando:  " << file_name << std::endl;
                FileInfo::delete_file(file_path);
            }
            else if (cmd == "list_server")
            {
                string folder_path = exec_path + "/" + folder_name;
                auto pkts = FileInfo::create_packet_vector("list_server_response", folder_path);
                send_queue.produce(pkts);
            }
            else if (cmd == "exit")
            {
                std::cout << "Encerrando conexão com o cliente." << std::endl;
                auto pkts = FileInfo::create_packet_vector("exit_response", "");
                send_queue.produce(pkts);
            }
        }
    }
}

void Server::create_sync_dir(int client_fd)
{
    std::string username = getUsername(client_fd);
    std::string dir_path = "sync_dir_" + username;
    FileInfo::create_dir(dir_path);
}

void Server::handle_sync(int &client_sock, std::string folder_name, Threads::AtomicQueue<std::vector<Packet>> &send_queue, Threads::AtomicQueue<std::vector<Packet>> &sync_queue)
{
    // std::cout << "LIDANDO COM SYNC" << std::endl;
    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();

    while (client_sock > 0)
    {
        auto packets = sync_queue.consume_blocking();
        packets[0].clean_payload();
        if (packets[0].get_type() == 2)
        {
            string cmd = packets[0].get_payload_as_string();
            if (cmd == "upload_sync")
            {
                string file_name = FileInfo::receive_file(packets, folder_name);
                std::cout << "Arquivo recebido: " << file_name << std::endl;
                string file_path = exec_path + "/" + folder_name + "/" + file_name;
                cout << "Arquivo pronto para envio via broadcast: " << file_path << endl;
                auto pkts = FileInfo::create_packet_vector("upload_broadcast", file_path);
                send_queue.produce(pkts);
            }
            else if (cmd == "delete_sync")
            {
                FileInfo file_info = FileInfo::receive_file_info(packets);
                string file_name = file_info.get_file_name();
                string file_path = exec_path + "/" + folder_name + "/" + file_name;
                cout << "Delete recebido para o arquvio: " << file_name << endl;
                cout << "Enviando delete_broadcast: " << file_name << endl;
                auto pkts = FileInfo::create_packet_vector("delete_broadcast", file_name);
                send_queue.produce(pkts);
                FileInfo::delete_file(file_path);
                std::cout << "Arquivo deletado via sync: " << file_name << std::endl;
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
