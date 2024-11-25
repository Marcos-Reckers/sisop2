#include "clientClass.h"

Client::Client(string username, struct hostent *server, string server_port) : username(username), server(server), server_port(server_port), sock(), synced_files() {}

void Client::set_sock(int sock) { this->sock = sock; }

std::mutex recive_file_mutex;
std::mutex recive_delete_mutex;
std::mutex initial_sync_mutex;
std::mutex send_packets_mutex;
std::mutex recive_packets_mutex;

void Client::handle_connection()
{
    this->sock = this->connect_to_server();
    char buffer[256];
    recv(this->sock, buffer, 256, 0);
    if (strcmp(buffer, "exit") == 0)
    {
        std::cout << "Não é possível conectar mais do que 2 dispositivos simultâneos." << std::endl;
        this->sock = -1;
        return;
    }

    if (this->sock > 0)
    {
        // Codigo para deixar não bloqeante entre recv e send
        //  ===================================================================
        auto flags = ::fcntl(sock, F_GETFL, 0);
        if (flags == -1)
        {
            close(sock);
            cout << "Erro ao obter flags do socket." << std::endl;
            return;
        }
        flags |= O_NONBLOCK;
        auto fcntl_result = ::fcntl(sock, F_SETFL, flags);
        if (fcntl_result == -1)
        {
            close(sock);
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

        std::thread io_thread([this, &send_queue, &received_queue, &sync_queue]()
                              { this->handle_io(send_queue, received_queue, sync_queue); });

        // cria as threds
        //  ===================================================================
        // cria thread de comandos
        std::thread command_thread([this, &send_queue, &received_queue]()
                                   { this->send_commands(send_queue, received_queue); });
        // pega os arquivos do servidor e do cliente e sincroniza
        // ===================================================================
        cout << "Sincronizando diretórios..." << endl;
        get_sync_dir(send_queue, received_queue);
        cout << "Sincronização inicial concluída." << endl;
        //  ===================================================================
        //  criathread de sync
        std::thread sync_thread([this, &sync_queue]()
                                { this->handle_sync(sync_queue, "sync_dir", synced_files); });
        // cria thread de monitoramento
        std::thread monitor_thread([this, &send_queue]()
                                   { this->monitor_sync_dir("sync_dir", send_queue, synced_files); });
        // ===================================================================

        io_thread.join();
        sync_thread.join();
        command_thread.join();
        monitor_thread.join();
        close(this->sock);
        return;
    }
    else
    {
        cout << "Problema ao conectar com servidor!" << this->sock << endl;
        return;
    }
}

int16_t Client::connect_to_server()
{
    struct sockaddr_in serv_addr;
    // Cria o socket
    int curr_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (curr_sock < 0)
    {
        cout << "Erro ao criar socket" << endl;
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(server_port.c_str()));
    serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
    bzero(&(serv_addr.sin_zero), 8);

    // Tenta conectar ao servidor por 100 segundos
    int attempts = 0;
    while (attempts < 10)
    {
        if (connect(curr_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == 0)
        {
            std::string username_with_null = username + '\0';
            send(curr_sock, username_with_null.c_str(), username_with_null.size(), 0);
            return curr_sock;
        }
        else
        {
            cout << "Tentativa de conexão falhou, tentando novamente..." << endl;
            sleep(1); // Aguarda 1 segundo antes de tentar novamente
            attempts++;
        }
    }

    cout << "Falha na conexão TIMEOUT" << endl;
    close(sock);
    return -3;
}

void Client::handle_io(Threads::AtomicQueue<std::vector<Packet>> &send_queue, Threads::AtomicQueue<std::vector<Packet>> &received_queue, Threads::AtomicQueue<std::vector<Packet>> &sync_queue)
{
    vector<Packet> packets_to_recv_queue;
    vector<Packet> packets_to_sync_queue;

    while (this->sock > 0)
    {
        // consumir do send_queue e enviar para o servidor na sock
        auto maybe_packet = send_queue.consume();
        if (maybe_packet.has_value())
        {
            auto packet = maybe_packet.value();
            string dirty_payload = packet[0].get_payload_as_string();
            string clean_payload = dirty_payload.substr(0, dirty_payload.find('|'));
            cout << "Enviando comando: " << clean_payload << " do tipo: " << packet[0].get_type() << endl;
            for (auto pkt : packet)
            {
                //semaforo
                std::vector<uint8_t> packet_bytes = Packet::packet_to_bytes(pkt);
                sleep(1);
                ssize_t sent_bytes = FileInfo::sendAll(this->sock, packet_bytes.data(), packet_bytes.size(), 0);
                if (sent_bytes < 0)
                {
                    std::cerr << "Erro ao enviar pacote." << std::endl;
                }
                std::cout << "Enviado pacote " << pkt.get_seqn() << "/" << pkt.get_total_packets() << " do tipo: " << pkt.get_type() << " de tamanho: " << sent_bytes << std::endl;
            }
        }

        ssize_t total_bytes = Packet::packet_header_size() + MAX_PAYLOAD_SIZE;
        std::vector<uint8_t> packet_bytes(total_bytes);
        // ssize_t received_bytes = FileInfo::recvAll(this->sock, packet_bytes);
        sleep(1);
        ssize_t received_bytes = recv(this->sock, packet_bytes.data(), packet_bytes.size(), 0);

        if (received_bytes > 0)
        {
            Packet received_packet = Packet::bytes_to_packet(packet_bytes);
            if (received_packet.get_seqn() == 1)
            {
                received_packet.clean_payload();
                cout << "Recebendo comando: " << received_packet.get_payload_as_string() << " do tipo: " << received_packet.get_type() << endl;
            }
            cout << "Recebeu pacote " << received_packet.get_seqn() << "/" << received_packet.get_total_packets() << " de tipo " << received_packet.get_type() << " de tamanho: " << received_bytes << endl;

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
            else if (received_packet.get_type() == 3)
            {
                std::cout << "Conexão encerrada." << std::endl;
                this->sock = -1;
                return;
            }
            else
            {
                std::cerr << "Pacote recebido com tipo inválido." << std::endl;
            }
        }
    }
    return;
}

void Client::get_sync_dir(Threads::AtomicQueue<std::vector<Packet>> &send_queue, Threads::AtomicQueue<std::vector<Packet>> &received_queue)
{
    std::lock_guard<std::mutex> lock(initial_sync_mutex);
    send_queue.produce(FileInfo::create_packet_vector("get_sync_dir"));
    FileInfo::create_dir("sync_dir");

    send_queue.produce(FileInfo::create_packet_vector("list_server"));
    vector<Packet> packets = received_queue.consume_blocking();
    vector<FileInfo> server_files = FileInfo::receive_list_server(packets);
    // FileInfo::print_list_files(server_files);

    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    std::string path = exec_path + "/" + "sync_dir";
    vector<FileInfo> client_files = FileInfo::list_files(path);

    vector<FileInfo> files_to_upload;
    for (auto client_file : client_files)
    {
        bool found = false;
        for (auto server_file : server_files)
        {
            if (client_file.get_file_name() == server_file.get_file_name())
            {
                found = true;
                string client_time = client_file.get_m_time();
                string server_time = server_file.get_m_time();
                int time = FileInfo::most_recent_time(client_time, server_time);
                if (time == 1)
                {
                    files_to_upload.push_back(client_file);
                }
            }
        }
        if (!found)
        {
            files_to_upload.push_back(client_file);
        }
    }

    for (auto file : files_to_upload)
    {
        string file_path = path + "/" + file.get_file_name();
        cout << "Enviando arquivo: " << file_path << endl;
        send_queue.produce(FileInfo::create_packet_vector("upload_sync", file_path));
    }

    vector<FileInfo> files_to_download;
    for (auto server_file : server_files)
    {
        bool found = false;
        for (auto client_file : client_files)
        {
            if (server_file.get_file_name() == client_file.get_file_name())
            {
                found = true;
                string client_time = client_file.get_m_time();
                string server_time = server_file.get_m_time();
                if (FileInfo::most_recent_time(client_time, server_time) == 2)
                {
                    files_to_download.push_back(server_file);
                }
            }
        }
        if (!found)
        {
            files_to_download.push_back(server_file);
        }
    }

    for (auto file : files_to_download)
    {
        send_queue.produce(FileInfo::create_packet_vector("download", file.get_file_name()));
        auto download_packets = received_queue.consume_blocking();
        FileInfo::receive_file(download_packets, "sync_dir");
        synced_files.insert(file.get_file_name());
        cout << "Arquivo recebido com sucesso." << endl;
    }
}

void Client::handle_sync(Threads::AtomicQueue<std::vector<Packet>> &sync_queue, string folder_name, set<string> &synced_files)
{
    std::cout << "A thread para lidar com sincronização no cliente está executando." << std::endl;
    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();

    while (this->sock > 0)
    {
        auto packets = sync_queue.consume_blocking();
        // Verifica se há pacotes
        if (packets.empty())
        {
            continue;
        }

        // Limpa e obtém o comando do primeiro pacote
        packets[0].clean_payload();
        string cmd = packets[0].get_payload_as_string();
        cout << "Comando recebido: " << cmd << endl;

        if (packets[0].get_type() == 2)
        {
            if (cmd.find("upload") != string::npos)
            {
                std::lock_guard<std::mutex> lock(recive_file_mutex);
                string file_name = FileInfo::receive_file(packets, folder_name);
                std::cout << "Arquivo recebido via upload sync: " << file_name << std::endl;
                cout << "Adicionando ao sync exclude: " << file_name << endl;
                synced_files.insert(file_name);
            }
            else if (cmd.find("delete") != string::npos)
            {
                if (packets.size() >= 2)
                {
                    packets[1].clean_payload();
                    FileInfo file_info = Packet::string_to_info(packets[1].get_payload());
                    string file_name = file_info.get_file_name();

                    if (!file_name.empty())
                    {
                        std::lock_guard<std::mutex> lock(recive_delete_mutex);
                        string file_path = exec_path + "/" + folder_name + "/" + file_name;
                        cout << "Deletando arquivo: " << file_path << endl;
                        if (std::filesystem::exists(file_path))
                        {
                            FileInfo::delete_file(file_path);
                            std::cout << "Arquivo deletado: " << file_name << std::endl;
                            synced_files.insert(file_name);
                        }
                        else
                        {
                            std::cout << "Arquivo não encontrado: " << file_name << std::endl;
                        }
                    }
                }
                else
                {
                    cout << "Pacotes insuficientes para deletar o arquivo." << endl;
                }
            }
        }
    }
    return;
}

void Client::send_commands(Threads::AtomicQueue<std::vector<Packet>> &send_queue, Threads::AtomicQueue<std::vector<Packet>> &received_queue)
{
    std::cout << "A thread para lidar com comandos no cliente está executando." << std::endl;
    while (this->sock > 0)
    {
        std::string cmd;
        std::cout << "Digite um comando: " << std::flush;
        std::getline(std::cin, cmd);

        if (cmd.rfind("upload", 0) == 0)
        {
            std::string file_path = cmd.substr(7);
            if (!file_path.empty())
            {
                if (std::filesystem::exists(file_path))
                {
                    string file_name = std::filesystem::path(file_path).filename();
                    const auto send_file = FileInfo::create_packet_vector("upload", file_path);
                    send_queue.produce(send_file);
                    cout << "Enviando comando de upload com o arquivo: " << file_name << endl;
                }
                else
                {
                    std::cerr << "UPLOAD: Arquivo não encontrado." << std::endl;
                }
            }
            else
            {
                std::cerr << "Comando inválido. Uso: upload <path/filename.ext>" << std::endl;
            }
        }
        else if (cmd.rfind("delete", 0) == 0)
        {
            std::string file_name = cmd.substr(7);
            if (!file_name.empty())
            {
                send_queue.produce(FileInfo::create_packet_vector("delete", file_name));
                cout << "Arquivo deletado: " << file_name << endl;
            }
            else
            {
                std::cerr << "Comando inválido. Uso: delete <filename.ext>" << std::endl;
            }
        }
        else if (cmd.rfind("download", 0) == 0)
        {
            std::string file_name = cmd.substr(9);
            if (!file_name.empty())
            {
                // envia o comando e o nome do arquivo para a fila de pacotes a serem enviados
                send_queue.produce(FileInfo::create_packet_vector("download", file_name));
                auto packets = received_queue.consume_blocking();
                FileInfo::receive_file(packets, "downloads");
                cout << "Arquivo recebido com sucesso." << endl;
            }
            else
            {
                std::cerr << "Comando inválido. Uso: download <filename.ext>" << std::endl;
            }
        }
        else if (cmd.rfind("list_server", 0) == 0)
        {
            send_queue.produce(FileInfo::create_packet_vector("list_server"));
            auto packets = received_queue.consume_blocking();
            if (packets.size() < 2)
            {
                std::cout << "Pasta do servidor vazia" << std::endl;
            }
            vector<FileInfo> file_infos = FileInfo::receive_list_server(packets);
            FileInfo::print_list_files(file_infos);
        }
        else if (cmd.rfind("list_client", 0) == 0)
        {
            std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
            std::string path = exec_path + "/" + "sync_dir";
            vector<FileInfo> files = FileInfo::list_files(path);
            FileInfo::print_list_files(files);
        }
        else if (cmd.rfind("exit", 0) == 0)
        {
            send_queue.produce(FileInfo::create_packet_vector("exit"));
            break;
        }
        else
        {
            std::cerr << "Comando inválido." << std::endl;
        }
    }

    return;
}

void Client::monitor_sync_dir(string folder_name, Threads::AtomicQueue<std::vector<Packet>> &send_queue, set<string> &synced_files)
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

    while (this->sock > 0)
    {
        int payload_size = read(fd, buffer, buf_size);
        if (payload_size < 0)
        {
            perror("read");
            break;
        }

        int i = 0;
        while (i < payload_size)
        {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];
            if (event->len)
            {
                if (event->mask & IN_CLOSE_WRITE || event->mask & IN_MOVED_TO)
                {
                    std::lock_guard<std::mutex> lock(recive_file_mutex); // Adquire o lock
                    if (synced_files.find(event->name) == synced_files.end())
                    {
                        string file_name = event->name;
                        cout << "Arquivo precisa ser syncronizado: " << file_name << endl;
                        string file_path = sync_dir + "/" + file_name;
                        cout << "Enviando arquivo para o servidor" << endl;
                        send_queue.produce(FileInfo::create_packet_vector("upload_sync", file_path));
                    }
                    else if (synced_files.find(event->name) != synced_files.end())
                    {
                        cout << "Arquivo já sincronizado: " << event->name << " removendo da lista para futuras alterações" << endl;
                        synced_files.erase(event->name);
                    }
                }
                if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM)
                {
                    std::lock_guard<std::mutex> lock(recive_delete_mutex);
                    if (synced_files.find(event->name) == synced_files.end())
                    {
                        string file_name = event->name;
                        cout << "Arquivo deletado: " << file_name << endl;
                        cout << "Enviando delete " << file_name << " para o servidor" << endl;
                        send_queue.produce(FileInfo::create_packet_vector("delete_sync", file_name));
                    }
                    else if (synced_files.find(event->name) != synced_files.end())
                    {
                        cout << "Arquivo já sincronizado: " << event->name << " removendo da lista para futuras alterações" << endl;
                        synced_files.erase(event->name);
                    }
                }
            }
            i += sizeof(struct inotify_event) + event->len;
        }
    }

    delete[] buffer;
    inotify_rm_watch(fd, wd);
    close(fd);
    return;
}
