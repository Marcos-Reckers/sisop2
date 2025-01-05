#include "serverClass.h"
#include <mutex>

std::mutex send_packets_mutex;
std::mutex recive_packets_mutex;
std::mutex add_client_mutex;

std::mutex bully_mutex;

// Construtor da classe que recebe a porta e tipo como argumento
Server::Server(int port, string type) : server_fd(-1), port(port), type(type)
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

            cout << "MANDANDO OK PARA: " << username_str << endl;
            send(client_fd, "ok", 2, 0);

            add_client_mutex.lock();

            addClient(client_fd, username_str);
            clients_info.push_back(ClientInfo{client_fd, username_str, client_addr});

            // print everything in clients_info
            std::cout << "Clients info: " << std::endl;
            for (auto client : clients_info)
            {
                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(client.addr.sin_addr), client_ip, INET_ADDRSTRLEN);
                std::cout << "SOCK: " << client.sock << " USERNAME: " << client.username << " ADDRESS: " << client_ip << ":" << ntohs(client.addr.sin_port) << std::endl;
            }

            add_client_mutex.unlock();

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

void Server::connect_server(string main_ip_address, string main_port)
{
    std::cout << "Entrei connect_server" << std::endl;
    struct sockaddr_in serv_addr;
    // Cria o socket
    int bully_curr_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (bully_curr_sock < 0)
    {
        std::cout << "Erro ao criar socket" << endl;
        return;
    }

    main_server = gethostbyname(main_ip_address.c_str());

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(main_port.c_str()));
    serv_addr.sin_addr = *((struct in_addr *)main_server->h_addr);
    bzero(&(serv_addr.sin_zero), 8);

    // Tenta conectar ao servidor por 100 segundos
    int attempts = 0;
    while (attempts < 10)
    {
        std::cout << "Dentro do while connect_server" << endl;
        if (connect(bully_curr_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == 0)
        {
            std::string username_with_null = this->backup_name;
            send(bully_curr_sock, username_with_null.c_str(), username_with_null.size(), 0);
            int recebido = 0;
            char buffer[3];

            while (recebido == 0)
            {
                recebido = recv(bully_curr_sock, buffer, 3, 0);
            }

            cout << "recebido primeiro ok: " << endl;
            for (auto c : buffer)
            {
                std::cout << c;
            }

            if (strstr(buffer, "ok") != NULL)
            {
                std::cout << "Conexão estabelecida com o servidor principal" << endl;
                thread maintain_connection(&Server::heartbeat, this, bully_curr_sock);

                thread backup_communication(&Server::handle_communication, this, bully_curr_sock);

                maintain_connection.join();

                bully_mutex.lock();

                bully();

                bully_mutex.unlock();

                if (this->type == "-b")
                {
                    std::cout << "Sou um backup" << endl;

                    std::cout << "depois do backup" << std::endl;
                    int recebido_betinha = 0;
                    char buffer_betinha[3];

                    while (recebido_betinha == 0)
                    {
                        recebido_betinha = recv(bully_curr_sock, buffer_betinha, 3, 0);
                    }

                    cout << "recebido segundo ok: " << endl;
                    for (auto c : buffer_betinha)
                    {
                        std::cout << c;
                    }

                    std::thread betinha(&Server::handle_communication, this, this->new_backup_sock);
                    std::thread maintain_connection(&Server::heartbeat, this, this->new_backup_sock);
                    maintain_connection.join();
                    bully_mutex.lock();
                    bully();
                    sleep(3);
                    bully_mutex.unlock();
                    if (this->type == "-p")
                    {
                        std::cout << "Sou um servidor principal" << endl;

                        clients_info.erase(std::remove_if(
                                               clients_info.begin(), clients_info.end(), [this](const ClientInfo &client)
                                               { return client.username == this->backup_name; }),
                                           clients_info.end());

                        thread connecting_to_clients(&Server::connect_clients, this);
                        connecting_to_clients.join();
                    }

                    betinha.join();
                }

                else
                {
                    std::cout << "Sou um servidor principal" << endl;

                    clients_info.erase(std::remove_if(
                                           clients_info.begin(), clients_info.end(), [this](const ClientInfo &client)
                                           { return client.username == this->backup_name; }),
                                       clients_info.end());

                    for (auto client : clients_info)
                    {
                        std::cout << "client_info: " << client.username << std::endl;
                    }

                    thread connecting_to_clients(&Server::connect_clients, this);
                    connecting_to_clients.join();
                }

                backup_communication.join();
            }

            else
            {
                std::cout << "Conexão recusada pelo servidor principal" << endl;
                close(bully_curr_sock);
                return;
            }
        }
        else
        {
            std::cout << "Tentativa de conexão falhou, tentando novamente..." << endl;
            sleep(1);
            attempts++;
        }
    }

    std::cout << "Falha na conexão TIMEOUT" << endl;
    close(bully_curr_sock);
    return;
}

// void Server::sync_servers(int &bully_curr_sock)
// {
//     return;
// }

void Server::connect_clients()
{
    // servidor vai atrás dos clientes no clients_info
    // connect pra cada um deles
    // abre todas as threads pra cada um deles
    std::cout << "tamanho do clients_info: " << clients_info.size() << std::endl;

    std::cout << "REMOVENDO TODOS MENOS BACKUP DA CLIENTS" << std::endl;

    for (auto client : clients)
    {
        std::cout << "SOCK: " << client.first << " USERNAME: " << client.second << std::endl;
        std::cout << "removendo: " << client.second << std::endl;
        removeClient(client.first);
        std::cout << "dps de remover" << std::endl;
    }

    std::cout << "printando dps de remover: " << std::endl;
    for (auto client : clients)
    {
        std::cout << "SOCK: " << client.first << " USERNAME: " << client.second << std::endl;
    }

    for (auto client : clients_info)
    {

        std::cout << "TENTANDO SE CONECTAR COM O CLIENTE: " << std::endl;

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client.addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        std::cout << "meu username eh: " << client.username << " sock: " << client.sock << " addr: " << client_ip << ":" << ntohs(client.addr.sin_port) << std::endl;

        if (client.username == this->backup_name)
        {
            continue;
        }

        if (client.username.find("BACKUP") == std::string::npos)
        {

            client.addr.sin_port = htons(atoi("8080"));

            int client_sock = socket(AF_INET, SOCK_STREAM, 0);
            if (client_sock < 0)
            {
                cout << "Erro ao criar socket" << endl;
                return;
            }

            // Tenta conectar ao servidor por 100 segundos
            int attempts = 0;
            while (attempts < 10)
            {
                if (connect(client_sock, (struct sockaddr *)&client.addr, sizeof(client.addr)) == 0)
                {
                    std::cout << "Conexão estabelecida com o cliente " << client.username << std::endl;

                    client.sock = client_sock;

                    addClient(client_sock, client.username);

                    client_threads.emplace_back(&Server::handle_communication, this, client_sock);
                    attempts = 10;
                }
                else
                {
                    cout << "Tentativa de conexão falhou, tentando novamente..." << endl;
                    sleep(1); // Aguarda 1 segundo antes de tentar novamente
                    attempts++;
                }
            }
            client.addr.sin_port = htons(atoi("8081"));
            // Tenta conectar ao servidor por 100 segundos
            attempts = 0;
            while (attempts < 10)
            {
                if (connect(client_sock, (struct sockaddr *)&client.addr, sizeof(client.addr)) == 0)
                {
                    std::cout << "Conexão estabelecida com o cliente " << client.username << std::endl;

                    client.sock = client_sock;

                    addClient(client_sock, client.username);

                    client_threads.emplace_back(&Server::handle_communication, this, client_sock);
                    attempts = 10;
                }
                else
                {
                    cout << "Tentativa de conexão falhou, tentando novamente..." << endl;
                    sleep(1); // Aguarda 1 segundo antes de tentar novamente
                    attempts++;
                }
            }
        }
    }

    std::cout << "printando dps DE TUDOOOOO " << std::endl;
    for (auto client : clients)
    {
        std::cout << "SOCK: " << client.first << " USERNAME: " << client.second << std::endl;
    }
}

bool Server::is_socket_open(int &bully_curr_sock)
{
    char buffer;

    int result = recv(bully_curr_sock, &buffer, 1, MSG_PEEK);
    std::cout << "result da sock do servidor p servidor: " << result << std::endl;

    if (result == 0)
    {
        return false;
    }

    else if (result < 0)
    {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
        {
            return true;
        }
        else
        {
            perror("recv");
            return false;
        }
    }
    return true;
}

void Server::heartbeat(int bully_curr_sock)
{
    std::cout << "entrei heartbeat" << std::endl;

    while (true)
    {
        if (!this->is_socket_open(bully_curr_sock))
        {
            std::cout << "HEARTBEAT PAROU" << std::endl;
            break;
        }
    }
    return;
}

void Server::handle_io(int &client_sock, Threads::AtomicQueue<std::vector<Packet>> &send_queue, Threads::AtomicQueue<std::vector<Packet>> &received_queue, Threads::AtomicQueue<std::vector<Packet>> &sync_queue)
{
    vector<Packet> packets_to_recv_queue;
    vector<Packet> packets_to_sync_queue;

    while (client_sock > 0)
    {
        if (type != "-b")
        {
            // consumir do send_queue e enviar para o servidor na sock
            auto maybe_packet = send_queue.consume();
            if (maybe_packet.has_value())
            {
                auto packet = maybe_packet.value();

                // ENVIA PRO OUTRO CLIENTE QUE NAO O QUE MANDOU
                if (packet[0].get_type() == 4)
                {
                    std::cout << "ENVIANDO DIRETO PRO BACKUP" << std::endl;

                    vector<int> backup_sockets = getUserSockets("BACKUP");

                    for (auto socket : backup_sockets)
                    {
                        for (auto pkt : packet)
                        {
                            pkt.set_type(2);
                            std::vector<uint8_t> packet_bytes = Packet::packet_to_bytes(pkt);
                            ssize_t sent_bytes = FileInfo::sendAll(socket, packet_bytes.data(), packet_bytes.size(), 0);
                            if (sent_bytes < 0)
                            {
                                std::cerr << "Erro ao enviar pacote." << std::endl;
                            }
                            std::cout << "Enviado pacote " << pkt.get_seqn() << "/" << pkt.get_total_packets() << " de tamanho: " << sent_bytes << " via broadcast" << std::endl;
                        }
                    }

                    for (auto client : clients)
                    {
                        if ((getUsername(client_sock) == client.second && client.first != client_sock))
                        {
                            std::cout << "ENVIANDO PRO CLIENTE: " << getUsername(client_sock) << std::endl;

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

                // ENVIA SÓ PRA QUEM MANDOU
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
                else if (packet[0].get_type() == 6)
                {
                    std::cout << "ENVIANDO CLIENTES PRO BACKUP" << std::endl;

                    vector<int> backup_sockets = getUserSockets("BACKUP");
                    cout << "backup_sockets size: " << backup_sockets.size() << endl;
                    for (auto backup : backup_sockets)
                    {
                        cout << "backup: " << backup << endl;
                    }

                    for (auto socket : backup_sockets)
                    {
                        for (auto pkt : packet)
                        {
                            std::vector<uint8_t> packet_bytes = Packet::packet_to_bytes(pkt);
                            ssize_t sent_bytes = FileInfo::sendAll(socket, packet_bytes.data(), packet_bytes.size(), 0);
                            if (sent_bytes < 0)
                            {
                                std::cerr << "Erro ao enviar pacote." << std::endl;
                            }
                            std::cout << "Enviado pacote " << pkt.get_seqn() << "/" << pkt.get_total_packets() << " de tamanho: " << sent_bytes << " via broadcast" << std::endl;
                        }
                    }

                    continue;
                }

                // ENVIA PRA TODOS OS CLIENTES DE UM USUÁRIO
                else
                {
                    std::cout << "enviando para o backup" << std::endl;

                    vector<int> backup_sockets = getUserSockets("BACKUP");

                    for (auto socket : backup_sockets)
                    {
                        for (auto pkt : packet)
                        {
                            pkt.set_type(2);
                            std::vector<uint8_t> packet_bytes = Packet::packet_to_bytes(pkt);
                            ssize_t sent_bytes = FileInfo::sendAll(socket, packet_bytes.data(), packet_bytes.size(), 0);
                            if (sent_bytes < 0)
                            {
                                std::cerr << "Erro ao enviar pacote." << std::endl;
                            }
                            std::cout << "Enviado pacote " << pkt.get_seqn() << "/" << pkt.get_total_packets() << " de tamanho: " << sent_bytes << " via broadcast" << std::endl;
                        }
                    }
                    for (auto client : clients)
                    {
                        if (getUsername(client_sock) == client.second)
                        {
                            std::cout << "ENVIANDO PRO CLIENTE: " << getUsername(client_sock) << std::endl;

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

            removeClient(client_sock);

            if (this->type == "-p")
            {
                std::cout << "Conexão do cliente " << username << " encerrada." << std::endl;

                string packet_string = create_string_from_client_info(clients_info);
                std::cout << "Enviando informações do cliente APÓS REMOVER: " << packet_string << std::endl;

                auto packet_client_info = FileInfo::create_packet_vector(packet_string);
                send_queue.produce(packet_client_info);
                std::cout << "Pacote criado do client_info e enviado pra fila" << std::endl;

                std::cout << "Printando o pacote do client_info" << std::endl;
                for (auto pkt : packet_client_info)
                {
                    pkt.print();
                }
            }

            // aqui esta o perigo
            // clients_info.erase(std::remove_if(clients_info.begin(), clients_info.end(), [client_sock](ClientInfo &client_info)
            //                                   { return client_info.sock == client_sock; }),
            //                    clients_info.end());

            // ClientInfo *client = find_client_info(clients_info, client_sock);

            // for (auto client : clients_info)
            // {
            //     char client_ip[INET_ADDRSTRLEN];
            //     inet_ntop(AF_INET, &(client.addr.sin_addr), client_ip, INET_ADDRSTRLEN);
            //     std::cout << "SOCK: " << client.sock << " USERNAME: " << client.username << " ADDRESS: " << client_ip << ":" << ntohs(client.addr.sin_port) << std::endl;
            // }

            close(client_sock);
            // client_sock = -1;
        }
        else if (received_bytes > 0)
        {
            Packet received_packet = Packet::bytes_to_packet(packet_bytes);
            cout << "Recebeu pacote " << received_packet.get_seqn() << "/" << received_packet.get_total_packets() << " de tamanho: " << received_bytes << endl;
            cout << "pacote recebido: ";
            received_packet.print();
            cout << endl;
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
            else if (received_packet.get_type() == 6)
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
    std::cout << "entrei handle_communication" << std::endl;

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
            { this->handle_io(client_sock, send_queue, received_queue, sync_queue); });

        // Cria a pasta do cliente no servidor para sincronização
        // ===================================================================
        if (getUsername(client_sock).find("BACKUP") == std::string::npos)
        {
            create_sync_dir(client_sock);
        }
        else
        {
            cout << "Cliente é um backup" << endl;
        }

        // ===================================================================

        // cria as threds
        //  ===================================================================
        auto client_folder = "sync_dir_" + getUsername(client_sock);
        // cria thread de comandos
        std::thread command_thread([&client_sock, client_folder, &send_queue, &received_queue]()
                                   { Server::handle_commands(client_sock, client_folder, send_queue, received_queue); });
        //  criathread de sync
        std::thread sync_thread([this, &client_sock, client_folder, &send_queue, &sync_queue]()
                                { this->handle_sync(client_sock, client_folder, send_queue, sync_queue); });

        if (getUsername(client_sock).find("BACKUP") == std::string::npos && !clients_info.empty() && this->type == "-p")
        {

            // ClientInfo *client = find_client_info(clients_info, client_sock);
            string packet_string = create_string_from_client_info(clients_info);
            std::cout << "Enviando informações do cliente: " << packet_string << std::endl;

            auto packet_client_info = FileInfo::create_packet_vector(packet_string);
            send_queue.produce(packet_client_info);
            std::cout << "Pacote criado do client_info e enviado pra fila" << std::endl;

            std::cout << "Printando o pacote do client_info" << std::endl;
            for (auto pkt : packet_client_info)
            {
                pkt.print();
            }
        }

        // std::cout << "depois do if do cliente != backup" << std::endl;

        std::cout << "ESTOU PARADO ESPERANDO" << std::endl;

        io_thread.join();
        command_thread.join();
        sync_thread.join();

        // std::cout << "DEI JOIN EM TODAS THREADS" << std::endl;

        return;
    }
    else
    {
        std::cerr << "Erro ao aceitar conexão do cliente." << std::endl;
        return;
    }
}

string Server::create_string_from_client_info(vector<ClientInfo> &clients_info)
{
    string clients_info_str = "client_info;";
    for (auto client : clients_info)
    {
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client.addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        string client_info_str = std::to_string(client.sock) + ";" + client.username + ";" + client_ip + ";" + std::to_string(ntohs(client.addr.sin_port)) + ";";
        clients_info_str = clients_info_str + "-" + client_info_str;
        cout << "client_info_str: " << client_info_str << endl;
    }
    cout << "clients_info_str: " << clients_info_str << endl;
    return clients_info_str;
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
    if (username.find("BACKUP") == std::string::npos)
    {
        std::string dir_path = "sync_dir_" + username;
        FileInfo::create_dir(dir_path);
    }
    else
    {
        return;
    }
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

                if (this->type == "-p")
                {
                    auto pkts = FileInfo::create_packet_vector("upload_broadcast", file_path);
                    cout << "Arquivo pronto para envio via broadcast: " << file_path << endl;
                    send_queue.produce(pkts);
                }
            }
            else if (cmd == "delete_sync")
            {
                FileInfo file_info = FileInfo::receive_file_info(packets);
                string file_name = file_info.get_file_name();
                string file_path = exec_path + "/" + folder_name + "/" + file_name;
                cout << "Delete recebido para o arquvio: " << file_name << endl;

                if (this->type == "-p")
                {
                    cout << "Enviando delete_broadcast: " << file_name << endl;
                    auto pkts = FileInfo::create_packet_vector("delete_broadcast", file_name);
                    send_queue.produce(pkts);
                    FileInfo::delete_file(file_path);
                    std::cout << "Arquivo deletado via sync: " << file_name << std::endl;
                }
            }
        }
        if (packets[0].get_type() == 6)
        {
            packets[0].clean_payload();
            string clients_info_string = packets[0].get_payload_as_string();
            // monta a estrutura de clients_info a partir da string
            clients_info.clear();

            vector<vector<string>> clients_info_vector = FileInfo::split_string(clients_info_string);

            for (auto client_info : clients_info_vector)
            {
                int sock = std::stoi(client_info[0]);
                std::string username = client_info[1];

                struct sockaddr_in addr;
                addr.sin_family = AF_INET;
                addr.sin_addr.s_addr = inet_addr(client_info[2].c_str());
                addr.sin_port = htons(std::stoi(client_info[3]));
                clients_info.push_back(ClientInfo{sock, username, addr});
            }

            std::cout << "Clients info RECEBIDO NO BACKUP: " << std::endl;
            for (auto client : clients_info)
            {
                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(client.addr.sin_addr), client_ip, INET_ADDRSTRLEN);
                std::cout << "SOCK: " << client.sock << " USERNAME: " << client.username << " ADDRESS: " << client_ip << ":" << ntohs(client.addr.sin_port) << std::endl;
                add_client_mutex.lock();
                addClient(client.sock, client.username);
                add_client_mutex.unlock();
                if (getUsername(client_sock).find("BACKUP") == std::string::npos)
                {
                    create_sync_dir(client_sock);
                }
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

vector<int> Server::getUserSockets(string username)
{
    vector<int> sockets;

    for (const auto &client : clients)
    {
        if (client.second.find(username) != string::npos)
        {
            sockets.push_back(client.first);
        }
    }

    return sockets;
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

int Server::connect_backup_servers()
{
    int tentativas = 20;
    for (int i = 0; i < tentativas; i++)
    {
        sockaddr_in backup_addr;
        socklen_t backup_addr_len = sizeof(backup_addr);

        // Aceita a conexão do cliente
        int backup_fd = accept(server_fd, (struct sockaddr *)&backup_addr, &backup_addr_len);

        std::cout << "BACKUP FD: " << backup_fd << std::endl;

        if (backup_fd >= 0)
        {
            std::cout << "Conexão aceita" << std::endl;
            return backup_fd;
        }
        else
        {
            std::cerr << "Erro ao aceitar conexão do backup." << std::endl;
            sleep(1);
            continue;
        }
    }
    return -1;
}

void Server::bully()
{

    std::cout << "ENTREI NO BULLY" << std::endl;
    // map entre addr e bully_number de backup;
    std::map<int, sockaddr_in> backup_bully_info;

    std::vector<int> backup_sockets = getUserSockets("BACKUP");

    backup_sockets.erase(
        std::remove_if(backup_sockets.begin(), backup_sockets.end(),
                       [this](int socket)
                       { return getUsername(socket) == this->backup_name; }),
        backup_sockets.end());

    this->bully_number = this->backup_name.substr(6);

    for (auto socket : backup_sockets)
    {
        std::string backup_name = getUsername(socket);
        int backup_number = std::stoi(backup_name.substr(6));

        for (const auto &client : clients_info)
        {
            if (client.username == backup_name)
            {
                backup_bully_info[backup_number] = client.addr;
                break;
            }
        }
    }

    election(backup_bully_info);
}

void Server::election(std::map<int, sockaddr_in> backup_bully_info)
{
    if (backup_bully_info.empty())
    {
        std::cout << "backup_bully_info vazio sou o ultimo backup" << std::endl;
        this->type = "-p";
        return;
    }

    for (auto backup : backup_bully_info)
    {
        if ((stoi(this->bully_number)) > backup.first)
        {
            // receive and accept
            ClientInfo new_info = wait_connect_from_backup(backup.second);

            this->type = "-p";

            for (auto &client_info : clients_info)
            {
                if (client_info.addr.sin_addr.s_addr == new_info.addr.sin_addr.s_addr)
                {
                    client_info.sock = new_info.sock;
                    break;
                }

                for (auto &client : clients)
                {
                    if (client.second == client_info.username)
                    {
                        clients.erase(client.first);
                        clients[new_info.sock] = client_info.username;
                        break;
                    }
                }
            }

            char buffer[3];
            sleep(1);
            recv(new_info.sock, buffer, 3, 0);
            std::cout << "recebi: " << buffer << std::endl;
        }
        else
        {
            // connect and send
            int backup_sock = connect_to_backup(backup.second);
            std::cout << "Sai do connect_to_backup" << std::endl;
            std::cout << "BACKUP SOCK (betinha): " << backup_sock << std::endl;

            this->new_backup_sock = backup_sock;
            cout << "mandando ok" << endl;
            send(backup_sock, "ok", 3, 0);
        }
    }
}

int Server::connect_to_backup(sockaddr_in &backup_addr)
{
    // Cria o socket
    int bully_curr_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (bully_curr_sock < 0)
    {
        cout << "Erro ao criar socket" << endl;
        return -1;
    }

    // std::cout << "ANTES DE MUDAR" << std::endl;
    // std::cout << "endereço QUE TA TENTANDO SE CONECTAR: " << inet_ntoa(backup_addr.sin_addr) << std::endl;
    // std::cout << "porta QUE TA TENTANDO SE CONECTAR: " << ntohs(backup_addr.sin_port) << std::endl;

    backup_addr.sin_family = AF_INET;
    backup_addr.sin_port = htons(atoi("8080"));
    bzero(&(backup_addr.sin_zero), 8);

    // std::cout << "DPS DE MUDAR A PORTA PRA 8080" << std::endl;
    // std::cout << "endereço QUE TA TENTANDO SE CONECTAR: " << inet_ntoa(backup_addr.sin_addr) << std::endl;
    // std::cout << "porta QUE TA TENTANDO SE CONECTAR: " << ntohs(backup_addr.sin_port) << std::endl;

    // Tenta conectar ao servidor por 100 segundos
    int attempts = 0;
    while (attempts < 10)
    {
        if (connect(bully_curr_sock, (struct sockaddr *)&backup_addr, sizeof(backup_addr)) == 0)
        {
            cout << "DO BACKUP TENTANDO CONECTAR: Conectado ao backup!" << endl;
            std::cout << "A SOCK DO BACKUP EH: " << bully_curr_sock << std::endl;

            return bully_curr_sock;
        }
        else
        {
            cout << "Tentativa de conexão falhou, tentando novamente..." << endl;
            sleep(1); // Aguarda 1 segundo antes de tentar novamente
            attempts++;
        }
    }

    cout << "Falha na conexão TIMEOUT" << endl;
    return -3;
}

ClientInfo Server::wait_connect_from_backup(sockaddr_in &backup_addr)
{
    ClientInfo tmp;
    tmp.username = "ruim";
    port = 8080;
    std::cout << "ESPERANDO CONEXÃO DO BACKUP" << std::endl;

    int new_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (new_sock == -1)
    {
        std::cerr << "Erro ao criar o socket DO BACKUP ESPERANDO CONEXÃO." << std::endl;
        return tmp;
    }

    int opt = 1;
    if (setsockopt(new_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        std::cerr << "Erro ao configurar SO_REUSEADDR NO BACKUP ESPERANDO CONEXÃO." << std::endl;
        close(new_sock);
        return tmp;
    }

    memset(&backup_addr, 0, sizeof(backup_addr));
    backup_addr.sin_family = AF_INET;
    backup_addr.sin_addr.s_addr = INADDR_ANY;
    backup_addr.sin_port = htons(port);

    if (bind(new_sock, (struct sockaddr *)&backup_addr, sizeof(backup_addr)) < 0)
    {
        std::cerr << "DO BACKUP ESPERANDO CONEXÃO: Erro ao fazer o bind na porta: " << backup_addr.sin_port << "." << std::endl;
    }
    else
    {
        std::cout << "DO BACKUP ESPERANDO CONEXÃO: Bind realizado com sucesso na porta: " << port << std::endl;
    }

    if (listen(new_sock, 1) < 0)
    {
        std::cerr << "DO BACKUP ESPERANDO CONEXÃO: Erro ao colocar o servidor em modo de escuta." << std::endl;
        close(new_sock);
        return tmp;
    }

    sockaddr_in server_addr;
    socklen_t server_len = sizeof(server_addr);
    int bully_sock = -1;

    while (true)
    {
        std::cout << "tentando aceitar conexao: " << std::endl;
        bully_sock = accept(new_sock, (struct sockaddr *)&server_addr, &server_len);
        if (bully_sock >= 0)
        {
            std::cout << "DO BACKUP ESPERANDO CONEXÃO: Conectou um novo servidor na sock: " << bully_sock << std::endl;
            break;
        }
        else
        {
            std::cerr << "DO BACKUP ESPERANDO CONEXÃO: Erro ao aceitar conexão. Tentando novamente..." << std::endl;
            sleep(1); // Aguarda 1 segundo antes de tentar novamente
        }
    }

    ClientInfo client_info;
    client_info.sock = bully_sock;
    client_info.addr = server_addr;
    client_info.username = "BACKUP";

    return client_info;
}

// void Server::last_backup()
// {
//     std::cout << "Sou um servidor principal" << endl;
//     this->type = "-p";

//     for (auto client : clients)
//     {
//         if (client.second.find("BACKUP") != std::string::npos)
//         {
//             clients.erase(client.first);
//         }
//     }
//     clients_info.erase(std::remove_if(
//                                            clients_info.begin(), clients_info.end(), [this](const ClientInfo &client)
//                                            { return client.username == this->backup_name; }),
//                                        clients_info.end());

//     thread connecting_to_clients(&Server::connect_clients, this);
//     connecting_to_clients.join();
// }

// void Server::answer(int backup_sock)
// {
// }

// void Server::bully()
// {
//     std::string election = "election: ";
//     std::string coord = "coord: ";
//     std::string answer = "answer: ";

//     char buffer[256];

//     bool im_bigger = true;

//     std::vector<std::tuple<sockaddr_in, int>> backup_bully_info;

//     while (true)
//     {
//         std::cout << "ENTREI NO WHILE DO BULLY" << std::endl;
//         std::vector<int> backup_sockets = getUserSockets("BACKUP");

//         backup_sockets.erase(
//             std::remove_if(backup_sockets.begin(), backup_sockets.end(),
//                            [this](int socket)
//                            { return getUsername(socket) == this->backup_name; }),
//             backup_sockets.end());

//         this->bully_number = this->backup_name.substr(6);

//         for (auto socket : backup_sockets)
//         {
//             std::string backup_name = getUsername(socket);
//             int backup_number = std::stoi(backup_name.substr(6));

//             if (backup_number > std::stoi(this->bully_number))
//             {
//                 for (const auto &client : clients_info)
//                 {
//                     if (client.username == backup_name)
//                     {
//                         backup_bully_info.push_back(std::make_tuple(client.addr, backup_number));
//                         break;
//                     }
//                 }
//             }
//         }

//         if (backup_bully_info.empty())
//         {
//             // accept new connections
//             // send answer

//             std::cout << "EU SOU O LIDER" << std::endl;

//             std::cout << "SLEEP POR 5 SEGUNDOS DO BACKUP BULLY INFO EMPTY" << std::endl;
//             sleep(5);

//             std::cout << "CONECTANDO COM OS BACKUP SERVERS" << std::endl;
//             int sock = connect_backup_servers();

//             std::cout << "SAI DO SLEEP DO BACKUP BULLY INFO EMPTY" << std::endl;

//             int recv_len = recv(sock, buffer, sizeof(buffer), 0);
//             if (recv_len > 0)
//             {
//                 buffer[recv_len] = '\0';
//                 std::cout << "RECEBI: " << buffer << std::endl;
//                 send(sock, &answer, answer.size(), 0);
//             }
//             else
//             {
//                 std::cout << "Timeout or error receiving data on bigger" << std::endl;
//                 break;
//             }

//             std::cout << "SAI DO SLEEP DO BACKUP BULLY INFO EMPTY E RECEBI!" << std::endl;

//             this->type = "-p";
//             return;
//         }

//         else
//         {
//             for (auto backup : backup_bully_info)
//             {
//                 std::cout << "BACKUP: " << std::get<1>(backup) << std::endl;

//                 if (stoi(this->bully_number) < std::get<1>(backup))
//                 {
//                     std::string message = election + this->bully_number;
//                     std::cout << "ENVIANDO: " << message << std::endl;

//                     int curr_sock_bully = socket(AF_INET, SOCK_STREAM, 0);
//                     if (curr_sock_bully < 0)
//                     {
//                         std::cout << "Erro ao criar socket" << endl;
//                         return;
//                     }

//                     int tentativas = 20;
//                     int sock;

//                     for (int i = 0; i < tentativas; i++)
//                     {
//                         sock = connect(curr_sock_bully, (struct sockaddr *)&std::get<0>(backup), sizeof(std::get<0>(backup)));
//                         if (sock != -1) // Check if connection is successful
//                         {
//                             std::cout << "CONECTOUUUUUUUUUUUUUUUUUUUUUU" << std::endl;
//                             break; // Exit the loop if connection is successful
//                         }
//                         else if (i == tentativas - 1) // If it's the last attempt and still failed
//                         {
//                             // Handle the failure case, e.g., log an error, throw an exception, etc.
//                             // For example:
//                             std::cerr << "Failed to connect after " << tentativas << " attempts." << std::endl;
//                             // Optionally, you can throw an exception or handle the error as needed
//                         }

//                         std::cout << "Tentando conectar com o backup, tentativa: " << i << std::endl;
//                         std::cout << "A SOCK É: " << sock << std::endl;
//                         sleep(1);
//                     }

//                     send(sock, &message, message.size(), 0);

//                     std::cout << "SLEEP POR 5 SEGUNDOS" << std::endl;
//                     sleep(5);

//                     std::cout << "SAI DO SLEEP" << std::endl;

//                     int recv_len = recv(sock, buffer, sizeof(buffer), 0);
//                     if (recv_len > 0)
//                     {
//                         buffer[recv_len] = '\0';
//                         std::cout << "RECEBI: " << buffer << std::endl;
//                     }
//                     else
//                     {
//                         std::cout << "Timeout or error receiving data ON SMALL" << std::endl;
//                         break;
//                     }

//                     std::cout << "SAI DO SLEEP E RECEBI!" << std::endl;
//                 }
//             }
//         }
//         std::cout << "continuo sendo um betinha backup" << std::endl;

//         std::cout << "SLEEP POR 5 SEGUNDOS ANTES DE SAIR" << std::endl;
//         sleep(5);

//         return;
//     }
// }
