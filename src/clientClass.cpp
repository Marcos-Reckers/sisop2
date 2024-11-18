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
    FileInfo::create_dir("sync_dir");

    FileInfo::send_cmd("list_server", sock);
    vector<FileInfo> server_files = FileInfo::receive_list_files(sock);
    FileInfo::print_list_files(server_files);

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
        FileInfo::send_cmd("upload", sock);
        string send_path =  path + "/" + file.get_file_name();
        cout << "Enviando arquivo: " << send_path << endl;
        FileInfo::send_file(send_path, sock);
        sleep(1);
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
        FileInfo::send_cmd("download", sock);
        FileInfo::send_file_name(file.get_file_name(), sock);
        FileInfo::receive_file("/sync_dir", sock);
    }
}

void Client::handle_sync()
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

        if (received_bytes == 0)
        {
            close(sock);
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
                handle_upload_request();
            }
            else if (cmd.substr(0, 6) == "delete")
            {
                handle_delete_request();
            }
        }
        else
        {
            std::cerr << "Pacote recebido não é do tipo 1." << std::endl;
        }
    }
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

void Client::handle_upload_request()
{
    string file_name = FileInfo::receive_file("sync_dir", sock);
    synced_files.insert(file_name);
}

void Client::handle_download_request()
{
    char file_name_buffer[256] = {0};
    ssize_t received_bytes = recv(sock, file_name_buffer, sizeof(file_name_buffer), 0);

    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    std::string file_path = exec_path + "sync_dir" + "/" + file_name_buffer;
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
        send(sock, error_packet_bytes.data(), error_packet_bytes.size(), 0);
        return;
    }

    FileInfo::send_file(file_path, sock);
}

void Client::handle_delete_request()
{
    char file_name_buffer[256] = {0};
    ssize_t received_bytes = recv(sock, file_name_buffer, sizeof(file_name_buffer), 0);
    if (received_bytes <= 0)
    {
        std::cerr << "Erro ao receber o nome do arquivo." << std::endl;
        return;
    }

    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    std::string path = exec_path + "sync_dir" + "/" + file_name_buffer;

    FileInfo::delete_file(path, sock);
}

void Client::monitor_sync_dir(string folder)
{

    FileInfo::create_dir(folder);

    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    std::string sync_dir = exec_path + "/" + folder;

    int fd = inotify_init();
    if (fd < 0)
    {
        perror("inotify_init");
        return;
    }

    int wd = inotify_add_watch(fd, sync_dir.c_str(), IN_CLOSE_WRITE | IN_DELETE | IN_MOVED_FROM);
    if (wd < 0)
    {
        perror("inotify_add_watch");
        close(fd);
        return;
    }

    const size_t buf_size = 1024 * (sizeof(struct inotify_event) + NAME_MAX + 1);
    char *buffer = new char[buf_size];

    while (true)
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
                    if (synced_files.find(event->name) != synced_files.end())
                    {
                        synced_files.erase(event->name);
                        break;
                    }
                    else if (synced_files.find(event->name) == synced_files.end())
                    {
                        string file_name = event->name;
                        cout << "Arquivo pronto para envio: " << file_name << endl;
                        FileInfo::send_cmd("upload", sock);
                        FileInfo::send_file(sync_dir + "/" + file_name, sock);
                    }
                }
                if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM)
                {
                    if (synced_files.find(event->name) != synced_files.end())
                    {
                        synced_files.erase(event->name);
                        break;
                    }
                    else if (synced_files.find(event->name) == synced_files.end())
                    {
                        string file_name = event->name;
                        cout << "Arquivo deletado: " << file_name << endl;
                        FileInfo::send_cmd("delete", sock);
                        FileInfo::send_file_name(file_name, sock);
                    }
                }
            }

            i += sizeof(struct inotify_event) + event->len;
        }
    }

    delete[] buffer;
    inotify_rm_watch(fd, wd);
    close(fd);
}