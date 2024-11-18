#include "fileInfo.h"
using namespace std;

FileInfo::FileInfo() : file_name(""), file_size(0), m_time(""), a_time(""), c_time("") {}
FileInfo::FileInfo(string file_name, int file_size, string m_time, string a_time, string c_time) : file_name(file_name), file_size(file_size), m_time(m_time), a_time(a_time), c_time(c_time) {}

void FileInfo::retrieve_info_from_file(string path)
{
    struct stat file_stat;

    if (stat(path.c_str(), &file_stat) != 0)
    {
        cout << "Não conseguiu pegar informações do arquivo" << endl;
    }

    file_name = filesystem::path(path).filename().string();
    file_size = file_stat.st_size;

    m_time = ctime(&file_stat.st_mtime);
    m_time.erase(remove(m_time.begin(), m_time.end(), '\n'), m_time.end());

    a_time = ctime(&file_stat.st_atime);
    a_time.erase(remove(a_time.begin(), a_time.end(), '\n'), a_time.end());

    c_time = ctime(&file_stat.st_ctime);
    c_time.erase(remove(c_time.begin(), c_time.end(), '\n'), c_time.end());
}

void FileInfo::print()
{
    cout << "Nome do arquivo: " << file_name << endl;
    cout << "Tamanho do arquivo: " << file_size << " bytes" << endl;
    cout << "Data de modificação: " << m_time << endl;
    cout << "Data de acesso: " << a_time << endl;
    cout << "Data de criação: " << c_time << endl;
}

void FileInfo::create_dir(string dir_name)
{
    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    std::string full_path = exec_path + "/" + dir_name;
    if (!std::filesystem::exists(full_path))
    {
        std::filesystem::create_directories(full_path); // Cria diretórios pai se não existirem
    }
}

string FileInfo::receive_file(std::string directory, int sock)
{
    std::cout << "Recebendo informações do arquivo..." << std::endl;

    FileInfo file_info = receive_file_info(sock);
    file_info.print();

    std::string file_name = file_info.get_file_name();
    int file_size = file_info.get_file_size();

    create_dir(directory);

    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    std::string save_path = exec_path + "/" + directory + "/" + file_name;

    std::ofstream outfile(save_path, std::ios::binary);

    if (!outfile.is_open())
    {
        std::cerr << "Erro ao abrir o arquivo: " << file_name << std::endl;
        return "";
    }

    int total_received_bytes = 0;

    while (total_received_bytes < file_size)
    {
        // Recebe o header do pacote
        size_t total_size = Packet::packet_base_size() + MAX_PAYLOAD_SIZE;
        std::vector<uint8_t> packet_buffer(total_size);
        
        ssize_t received_bytes = recv(sock, packet_buffer.data(), total_size, 0);
        if (received_bytes <= 0)
        {
            std::cerr << "Erro ao receber pacote." << std::endl;
            break;
        }

        Packet packet = Packet::bytes_to_packet(packet_buffer);
        
        if (packet.get_type() == 2) // Verifica se é pacote de dados
        {
            std::vector<char> payload = packet.get_payload();
            if (!payload.empty())
            {
                outfile.write(payload.data(), packet.get_length());
                total_received_bytes += packet.get_length();
                cout << "Recebidos " << total_received_bytes << " de " << file_size << " bytes" << endl;
            }
        }
    }

    outfile.close();

    if (total_received_bytes == file_size)
    {
        std::cout << "Arquivo recebido com sucesso." << std::endl;
        return file_name;
    }
    else
    {
        std::cerr << "Transferência de arquivo incompleta. Esperando " << file_size
                  << " bytes mas recebeu " << total_received_bytes << " bytes." << std::endl;
        return "";
    }
}

FileInfo FileInfo::receive_file_info(int sock)
{
    // Recebe o header do pacote
    size_t header_size = Packet::packet_base_size();
    std::vector<uint8_t> header_buffer(header_size);

    ssize_t received_bytes = recvAll(sock, header_buffer.data(), header_size, 0);
    if (received_bytes <= 0)
    {
        std::cerr << "Erro ao receber o header do pacote." << std::endl;
        return FileInfo();
    }

    Packet packet = Packet::bytes_to_packet(header_buffer);

    // Recebe o payload do pacote
    uint16_t payload_length = packet.get_length();
    if (payload_length > 0)
    {
        std::vector<char> payload_buffer(payload_length);
        received_bytes = recvAll(sock, payload_buffer.data(), payload_length, 0);
        if (received_bytes <= 0)
        {
            std::cerr << "Erro ao receber o payload do pacote." << std::endl;
            return FileInfo();
        }
        packet.set_payload(payload_buffer);
    }

    // Converte o payload em FileInfo
    FileInfo file_info = Packet::string_to_info(packet.get_payload());
    return file_info;
}

void FileInfo::send_file(string file_path, int sock)
{
    send_file_info(sock, file_path);

    std::vector<Packet> packets = Packet::create_packets_from_file(file_path);

    if (packets.empty())
    {
        std::cout << "Arquivo vazio." << std::endl;
        return;
    }

    // Envia conteudo do arquivo
    for (Packet packet : packets)
    {
        std::vector<uint8_t> packet_bytes = Packet::packet_to_bytes(packet);

        //ssize_t sent_bytes = send(sock, packet_bytes.data(), packet_bytes.size(), 0);
        ssize_t sent_bytes = sendAll(sock, packet_bytes.data(), packet_bytes.size(), 0);
        if (sent_bytes < 0)
        {
            std::cout << "Erro ao enviar pacote." << std::endl;
            return;
        }

        std::cout << "Pacote " << packet.get_seqn() << " de tamanho: " << sent_bytes << " bytes enviado." << std::endl;
    }

    std::cout << "Arquivo enviado com sucesso." << std::endl;
}

void FileInfo::send_file_name(string file_path, int sock)
{
    string file_name = std::filesystem::path(file_path).filename().string();
    std::vector<uint8_t> file_name_bytes(file_name.begin(), file_name.end());
    file_name_bytes.push_back('\0');

    ssize_t sent_bytes = send(sock, file_name_bytes.data(), file_name_bytes.size(), 0);
    if (sent_bytes < 0)
    {
        std::cerr << "Erro ao enviar nome do arquivo." << std::endl;
        return;
    }
    std::cout << "Nome do arquivo enviado: " << file_name << std::endl;
}

void FileInfo::send_file_info(int sock, std::string file_path)
{
    FileInfo file_info;

    file_info.retrieve_info_from_file(file_path);
    file_info.print();

    Packet packet = Packet::create_packet_info(file_info);

    std::vector<uint8_t> packet_bytes = Packet::packet_to_bytes(packet);
    ssize_t sent_bytes = send(sock, packet_bytes.data(), packet_bytes.size(), 0);

    if (sent_bytes < 0)
    {
        std::cerr << "Erro ao enviar informações do arquivo." << std::endl;
    }
}

vector<FileInfo> FileInfo::list_files(string path)
{
    vector<FileInfo> files = {};
    if (std::filesystem::is_empty(path))
    {
        cout << "Pasta vazia" << endl;
        return files;
    }
    else
    {
        for (const auto &entry : std::filesystem::directory_iterator(path))
        {
            FileInfo file_info;
            file_info.retrieve_info_from_file(entry.path());
            files.push_back(file_info);
        }
        return files;
    }
}

void FileInfo::send_list_files(vector<FileInfo> files, int socket)
{
    for (FileInfo file : files)
    {
        Packet packet = Packet::create_packet_info(file);
        std::vector<uint8_t> packet_bytes = Packet::packet_to_bytes(packet);
        ssize_t sent_bytes = send(socket, packet_bytes.data(), packet_bytes.size(), 0);

        if (sent_bytes < 0)
        {
            std::cerr << "Erro ao enviar informações do arquivo." << std::endl;
            return;
        }
        
        // Pequeno delay para evitar sobrecarregar o buffer
        usleep(1000);
    }

    // Envia pacote de finalização
    Packet end_packet = Packet::create_packet_cmd("END_OF_LIST");
    std::vector<uint8_t> end_bytes = Packet::packet_to_bytes(end_packet);
    send(socket, end_bytes.data(), end_bytes.size(), 0);
}

vector<FileInfo> FileInfo::receive_list_files(int sock)
{
    std::vector<FileInfo> files = {};
    bool end_received = false;

    // Remove timeout temporário para garantir recebimento completo
    struct timeval tv;
    tv.tv_sec = 5;  // 5 segundos de timeout
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    while (!end_received)
    {
        size_t total_size = Packet::packet_base_size() + MAX_PAYLOAD_SIZE;
        std::vector<uint8_t> packet_buffer(total_size);
        
        ssize_t received_bytes = recv(sock, packet_buffer.data(), packet_buffer.size(), 0);
        if (received_bytes <= 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;  // Timeout - assumimos que a lista terminou
            }
            std::cerr << "Erro ao receber lista de arquivos" << std::endl;
            break;
        }

        Packet packet = Packet::bytes_to_packet(packet_buffer);
        
        if (packet.get_type() == 1 && packet.get_payload_as_string() == "END_OF_LIST")
        {
            end_received = true;
            continue;
        }
        
        if (packet.get_type() == 3)  // Pacote de informação de arquivo
        {
            try {
                FileInfo file_info = Packet::string_to_info(packet.get_payload());
                files.push_back(file_info);
            } catch (const std::exception& e) {
                std::cerr << "Erro ao processar informações do arquivo: " << e.what() << std::endl;
            }
        }
    }

    // Restaura o socket para modo bloqueante
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    return files;
}

void FileInfo::print_list_files(vector<FileInfo> files)
{
    for (FileInfo file : files)
    {
        cout << "==============================" << endl;
        file.print();
        cout << "==============================" << endl;
    }
}

void FileInfo::delete_file(string file_path, int sock)
{

    if (!std::filesystem::exists(file_path))
    {
        std::cerr << "Erro ao deletar o arquivo." << std::endl;
        std::cerr << "Arquivo não encontrado." << std::endl;
        std::string error_msg = "ERROR: Arquivo não encontrado.";
        Packet error_packet = Packet::create_packet_cmd(error_msg);
        std::vector<uint8_t> error_packet_bytes = Packet::packet_to_bytes(error_packet);
        send(sock, error_packet_bytes.data(), error_packet_bytes.size(), 0);
        return;
    }
    else if (std::filesystem::remove(file_path))
    {
        std::cout << "Arquivo deletado com sucesso." << std::endl;
    }
}

void FileInfo::monitor_sync_dir(string folder, int sock) {

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
                if (event->mask & IN_CLOSE_WRITE) {
                    string file_name = event->name;
                        cout << "Arquivo pronto para envio: " << file_name << endl;
                        FileInfo::send_cmd("upload", sock);
                        FileInfo::send_file(sync_dir + "/" +  file_name, sock);
                }
                if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM) {
                    string file_name = event->name;
                        cout << "Arquivo deletado: " << file_name << endl;
                        FileInfo::send_cmd("delete", sock);
                        FileInfo::send_file_name(file_name, sock);
                        cout << file_name << " deletado" << endl;
                }
            }

            i += sizeof(struct inotify_event) + event->len;
        }
    }

    delete[] buffer;
    inotify_rm_watch(fd, wd);
    close(fd);
}

void FileInfo::send_cmd(std::string cmd, int sock)
{
    Packet cmd_packet = Packet::create_packet_cmd(cmd);
    std::vector<uint8_t> packet_bytes = Packet::packet_to_bytes(cmd_packet);
    ssize_t sent_bytes = send(sock, packet_bytes.data(), packet_bytes.size(), 0);

    if (sent_bytes < 0)
    {
        std::cerr << "Erro ao enviar comando como pacote." << std::endl;
        return;
    }

    std::cout << "Comando enviado como pacote: " << cmd << std::endl;
}

ssize_t FileInfo::sendAll(int sockfd, const void *buf, size_t len, int flags)
{
    size_t total = 0;
    const char *ptr = (const char *)buf;
    while (total < len)
    {
        ssize_t sent = send(sockfd, ptr + total, len - total, flags);
        if (sent <= 0)
            return sent;
        total += sent;
    }
    return total;
}

ssize_t FileInfo::recvAll(int sockfd, void *buf, size_t len, int flags)
{
    size_t total = 0;
    char *ptr = (char *)buf;
    while (total < len)
    {
        ssize_t received = recv(sockfd, ptr + total, len - total, flags);
        if (received <= 0)
            return received;
        total += received;
    }
    return total;
}