#include "clientClass.h"


Client::Client(string username, struct hostent *server, string server_port) : username(username), server(server), server_port(server_port) {}

void Client::set_sock(int sock) { this->sock = sock; }


int16_t Client::connect_to_server()
{
    struct sockaddr_in serv_addr;
    // Cria o socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
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
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == 0)
        {
            std::string username_with_null = username + '\0';
            send(sock, username_with_null.c_str(), username_with_null.size(), 0);

            if (end_connection())
            {
                cout << "Não é possível conectar mais do que 2 dispositivos simultâneos." << endl;
                return -2;
            }

            set_sock(sock);
            return sock;
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

void Client::get_sync_dir()
{
    FileInfo::send_cmd("get_sync_dir", sock);
}

bool Client::end_connection()
{
    char buffer[256];
    recv(sock, buffer, 256, 0);
    if (strcmp(buffer, "exit") == 0)
    {
        close(sock);
        return true;
    }
    return false;
}

void Client::handle_sync_request(int sock)
{
    while (true)
    {
        ssize_t total_size = Packet::packet_base_size() + MAX_PAYLOAD_SIZE;
        std::vector<uint8_t> packet_buffer(total_size);
        ssize_t received_bytes = recv(sock, packet_buffer.data(), packet_buffer.size(), 0);

        if (received_bytes < 0)
        {
            std::cerr << "Erro ao receber o pacote." << std::endl;
            return;
        }

        Packet pkt = Packet::bytes_to_packet(packet_buffer);

        if (pkt.get_type() == 1)
        {
            std::vector<char> cmd_vec = pkt.get_payload();
            std::string cmd(cmd_vec.begin(), cmd_vec.end());
            
            auto now = std::chrono::steady_clock::now();
            if ((now - last_sync) < std::chrono::seconds(1)) {
                continue;
            }
            
            std::cout << "Comando recebido: " << cmd << std::endl;
            if (cmd.substr(0, 6) == "upload")
            {
                handle_upload_request();
            }
            if (cmd.substr(0, 6) == "delete")
            {
                handle_delete_request();
            }
        }
    }
}

void Client::handle_sync(int sock)
{
    FileInfo::send_cmd("get_sync_dir", sock);
    monitor_sync_dir("sync_dir", sock); // Removido o Client: desnecessário
}

void Client::handle_upload_request()
{    
    string file_name = FileInfo::receive_file("/sync_dir/", sock);
    if (!file_name.empty()) {
        // Marca o arquivo como recebido do servidor e adiciona timestamp
        received_files[file_name] = 'S';
        last_sync = std::chrono::steady_clock::now();
    }
}

void Client::handle_delete_request()
{
    char file_name_buffer[256] = {0};
    ssize_t received_bytes = recv(sock, file_name_buffer, sizeof(file_name_buffer), 0);
    if(received_bytes <= 0)
    {
        std::cerr << "Erro ao receber o nome do arquivo." << std::endl;
        return;
    }

    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();\
    std::string path = exec_path + "/sync_dir/" + std::string(file_name_buffer);
    cout << path << " deletado" << endl;

    FileInfo::delete_file(path, sock);
}

void Client::monitor_sync_dir(string folder, int sock) {

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
                auto now = std::chrono::steady_clock::now();
                
                // Ignora eventos muito próximos ao último upload
                if ((now - last_upload) < std::chrono::seconds(2)) {
                    i += sizeof(struct inotify_event) + event->len;
                    continue;
                }

                if (event->mask & IN_CLOSE_WRITE) {
                    // Verifica se o arquivo veio do servidor recentemente
                    auto it = received_files.find(file_name);
                    if (it != received_files.end() && it->second == 'S' && 
                        (now - last_sync) < std::chrono::seconds(1)) {
                        continue;
                    }
                    
                    cout << "Arquivo pronto para envio: " << file_name << endl;
                    FileInfo::send_cmd("upload", sock);
                    FileInfo::send_file(sync_dir + "/" + file_name, sock);
                    last_upload = std::chrono::steady_clock::now();
                    received_files[file_name] = 'C';
                }
                
                if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM) {
                    auto it = received_files.find(file_name);
                    if (it != received_files.end()) {
                        if (it->second == 'S') {
                            received_files.erase(it);
                            continue;
                        }
                    }
                    
                    cout << "Arquivo deletado: " << file_name << endl;
                    FileInfo::send_cmd("delete", sock);
                    FileInfo::send_file_name(file_name, sock);
                    received_files.erase(file_name);
                }
            }

            i += sizeof(struct inotify_event) + event->len;
        }
    }

    delete[] buffer;
    inotify_rm_watch(fd, wd);
    close(fd);
}