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

string FileInfo::receive_file(Threads::AtomicQueue<std::vector<Packet>> &received_queue, string dst_folder)
{
    // Le da fila de pacotes recebidos e monta o arquivo:
    auto received_packet = received_queue.consume_blocking();
    FileInfo file_info = receive_file_info(received_packet);
    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    string file_name = file_info.get_file_name();
    std::string save_path = exec_path + "/" + dst_folder + "/" + file_name;
    std::ofstream outfile(save_path, std::ios::binary);
    if (!outfile.is_open())
    {
        std::cerr << "Erro ao abrir o arquivo: " << file_name << std::endl;
        return "";
    }
    // Ignora dois primeiros pacotes, que soh tem informacao
    int counter = 0;
    for (auto packet : received_packet)
    {
        if (counter < 1)
        {
            counter++;
            continue;
        }
        outfile.write(packet.get_payload().data(), packet.get_payload().size());
    }
    outfile.close();
    std::cout << "Arquivo recebido com sucesso." << std::endl;
    return file_name;
}

FileInfo FileInfo::receive_file_info(vector<Packet> &received_packet)
{
    FileInfo file_info = Packet::string_to_info(received_packet[1].get_payload());
    return file_info;
}

vector<FileInfo> FileInfo::receive_list_server(Threads::AtomicQueue<std::vector<Packet>> &received_queue)
{
    // Le da fila de pacotes recebidos e monta o arquivo:
    auto received_packet = received_queue.consume_blocking();
    vector<FileInfo> file_infos;
    int counter = 0;
    for (auto packet : received_packet)
    {
        // Ignora o primeiro pacote, que soh tem informacao
        if (counter <= 1)
        {
            counter++;
            continue;
        }
        FileInfo file_info = Packet::string_to_info(packet.get_payload());
        file_infos.push_back(file_info);
    }
    return file_infos;
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
        ssize_t sent_bytes = sendAll(socket, packet_bytes.data(), packet_bytes.size(), 0);

        if (sent_bytes < 0)
        {
            std::cerr << "Erro ao enviar informações do arquivo." << std::endl;
        }
        packet.print();
    }
    Packet packet = Packet::create_packet_cmd("END_OF_LIST");
    std::vector<uint8_t> packet_bytes = Packet::packet_to_bytes(packet);
    ssize_t sent_bytes = sendAll(socket, packet_bytes.data(), packet_bytes.size(), 0);

    if (sent_bytes < 0)
    {
        std::cerr << "Erro ao enviar informações do arquivo." << std::endl;
    }
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

void FileInfo::delete_file(string file_path)
{

    if (!std::filesystem::exists(file_path))
    {
        std::cerr << "Erro ao deletar o arquivo, NOT FOUND." << std::endl;
    }
    try
    {
        std::filesystem::remove(file_path);
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return;
    }
    std::cout << "Arquivo deletado com sucesso." << std::endl;
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

int FileInfo::most_recent_time(std::string time1, std::string time2) {
    struct tm tm1 = {}, tm2 = {};
    time_t time1_t, time2_t;

    // Parse first time string
    if (strptime(time1.c_str(), "%a %b %d %H:%M:%S %Y", &tm1) == NULL) {
        std::cerr << "Error: Failed to parse time1: " << time1 << std::endl;
        return -1;
    }

    // Parse second time string
    if (strptime(time2.c_str(), "%a %b %d %H:%M:%S %Y", &tm2) == NULL) {
        std::cerr << "Error: Failed to parse time2: " << time2 << std::endl;
        return -1;
    }

    // Convert to time_t
    time1_t = mktime(&tm1);
    time2_t = mktime(&tm2);

    if (time1_t == -1 || time2_t == -1) {
        std::cerr << "Error: Failed to convert tm to time_t." << std::endl;
        return -1;
    }

    // Compare times
    if (difftime(time1_t, time2_t) > 0) {
        return 1;       // time1 is more recent
    } else if (difftime(time1_t, time2_t) < 0) {
        return 2;       // time2 is more recent
    } else {
        return 0;       // times are equal
    }
}

vector<Packet> FileInfo::create_packet_vector(string command, string file_path_or_file_name)
{
    Packet pkt_cmd = Packet::create_packet_cmd(command);

    if (command == "upload")
    {
        FileInfo file_info;
        file_info.retrieve_info_from_file(file_path_or_file_name);
        Packet pkt_file_info = Packet::create_packet_info(file_info);

        std::vector<Packet> pkt_files = Packet::create_packets_from_file(file_path_or_file_name);
        pkt_files.insert(pkt_files.begin(), pkt_file_info);
        pkt_files.insert(pkt_files.begin(), pkt_cmd);
        return pkt_files;
    }
    else if (command == "delete")
    {
        FileInfo file_info;
        file_info.set_file_name(file_path_or_file_name);
        Packet pkt_file_info = Packet::create_packet_info(file_info);

        std::vector<Packet> pkts;
        pkts.push_back(pkt_cmd);
        pkts.push_back(pkt_file_info);

        return pkts;
    }
    else if (command == "download")
    {
        FileInfo file_info;
        file_info.set_file_name(file_path_or_file_name);
        Packet pkt_file_info = Packet::create_packet_info(file_info);

        std::vector<Packet> pkts;
        pkts.push_back(pkt_cmd);
        pkts.push_back(pkt_file_info);

        return pkts;   
    }
    else if (command == "list_server")
    {
        vector<Packet> solo_pkt;
        solo_pkt.push_back(pkt_cmd);
        return solo_pkt;
    }
    else if (command == "list_client")
    {
        vector<Packet> solo_pkt;
        solo_pkt.push_back(pkt_cmd);
        return solo_pkt;
    }
    else if (command == "exit")
    {
        vector<Packet> solo_pkt;
        solo_pkt.push_back(pkt_cmd);
        return solo_pkt;
    }
} 