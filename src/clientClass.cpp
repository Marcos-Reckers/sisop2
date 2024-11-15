#include "clientClass.h"

#include <sys/inotify.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <netdb.h>
#include <filesystem>
#include <thread>
#include <limits.h>

#include "packet.h"
#include "fileInfo.h"

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

void Client::send_cmd(string cmd)
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

void Client::send_file(string file_path)
{
    send_file_name(file_path);
    uint32_t file_size = std::filesystem::file_size(file_path);
    uint32_t file_size_network_order = htonl(file_size);
    ssize_t sent_bytes = send(sock, &file_size_network_order, sizeof(file_size_network_order), 0);
    if (sent_bytes < 0)
    {
        std::cerr << "Erro ao enviar o tamanho do arquivo." << std::endl;
        return;
    }
    std::cout << "Tamanho do arquivo: " << file_size << " bytes." << std::endl;

    std::vector<Packet> packets = Packet::create_packets_from_file(file_path);

    if (packets.empty())
    {
        cout << "Arquivo vazio." << endl;
        return;
    }

    for (Packet packet : packets)
    {
        std::vector<uint8_t> packet_bytes = Packet::packet_to_bytes(packet);

        ssize_t sent_bytes = send(sock, packet_bytes.data(), packet_bytes.size(), 0);
        if (sent_bytes < 0)
        {
            cout << "Erro ao enviar pacote." << endl;
            return;
        }

        std::cout << "Pacote " << packet.get_seqn() << " de tamanho: " << sent_bytes << " bytes enviado." << std::endl;
    }

    std::cout << "Arquivo enviado com sucesso." << std::endl;
}

void Client::create_dir(string dir_name)
{
    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    std::string full_path = exec_path + "/" + dir_name;
    if (!std::filesystem::exists(full_path))
    {
        std::filesystem::create_directory(full_path);
    }
}

void Client::get_sync_dir()
{
    send_cmd("get_sync_dir");
}

void Client::receive_file()
{
    std::cout << "Recebendo informações do arquivo..." << std::endl;

    FileInfo file_info = receive_file_info();

    string file_name = file_info.get_file_name();
    int file_size = file_info.get_file_size();

    create_dir("downloads");
    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    std::string save_path = exec_path + "/downloads/" + file_name;

    std::ofstream outfile(save_path, std::ios::binary);

    if (!outfile.is_open())
    {
        std::cerr << "Erro ao abrir o arquivo: " << file_name << std::endl;
        return;
    }

    ssize_t received_bytes = 0;
    int total_received_bytes = 0;
    while (total_received_bytes < file_size)
    {
        ssize_t total_size = Packet::packet_base_size() + MAX_PAYLOAD_SIZE;

        std::vector<uint8_t> packet_buffer(total_size);

        received_bytes = recv(sock, packet_buffer.data(), packet_buffer.size(), 0);
        if (received_bytes <= 0)
        {
            std::cerr << "Erro ao receber o pacote:" << std::endl;
            break;
        }

        Packet packet = Packet::bytes_to_packet(packet_buffer);
        if (packet.get_type() == 1 && packet.get_payload_as_string().substr(0, 5) == "ERROR")
        {
            std::cerr << "Erro do servidor: " << packet.get_payload_as_string() << std::endl;
            return;
        }

        outfile.write(packet.get_payload().data(), packet.get_length());

        total_received_bytes += packet.get_length();

        std::cout << "Pacote recebido de tamanho: " << received_bytes << " bytes." << std::endl;
    }

    outfile.close();

    if (total_received_bytes == file_size)
    {
        std::cout << "Arquivo recebido com sucesso." << std::endl;
    }
    else
    {
        std::cerr << "Transferência de arquivo incompleta. Esperando " << file_size << " bytes mas recebeu " << total_received_bytes << " bytes." << std::endl;
    }
}

FileInfo Client::receive_file_info()
{
    std::vector<uint8_t> packet_buffer(Packet::packet_base_size() + MAX_PAYLOAD_SIZE);
    ssize_t received_bytes = recv(sock, packet_buffer.data(), packet_buffer.size(), 0);
    if (received_bytes <= 0)
    {
        std::cerr << "Erro ao receber o pacote:" << std::endl;
        return FileInfo();
    }

    Packet packet = Packet::bytes_to_packet(packet_buffer);
    packet.print();

    FileInfo file_info = Packet::string_to_info(packet.get_payload());
    return file_info;
}

void Client::send_file_name(string file_path)
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

void Client::list_files_client()
{
    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    std::string path = exec_path + "/sync_dir";

    if (std::filesystem::is_empty(path))
    {
        cout << "==============================" << endl;
        cout << "Diretório vazio" << endl;
        cout << "==============================" << endl;
        return;
    }
    
    for (const auto &entry : std::filesystem::directory_iterator(path))
    {
        FileInfo file_info;
        file_info.retrieve_info_from_file(entry.path());
        //formata a saida para o cliente
        cout << "==============================" << endl;
        file_info.print();
        cout << "==============================" << endl;
    }
}

void Client::list_files_server()
{
    send_cmd("list_server");
    float counter = 0;

    while (true)
    {
        std::vector<uint8_t> packet_buffer(Packet::packet_base_size() + MAX_PAYLOAD_SIZE);
        ssize_t received_bytes = recv(sock, packet_buffer.data(), packet_buffer.size(), 0);
        if (received_bytes <= 0)
        {
            std::cerr << "Erro ao receber o pacote." << std::endl;
            break;
        }

        Packet packet = Packet::bytes_to_packet(packet_buffer);

        // Verifica se o pacote recebido é o pacote de fim de lista
        if (packet.get_type() == 1 && packet.get_payload_as_string() == "END_OF_LIST")
        {
            packet.print();

            if (counter == 0)
            {
                cout << "Lista de arquivos vazia." << endl;
            }
            else
            {
                cout << "Fim da lista de arquivos." << endl;
            }

            return;
        }

        FileInfo file_info = Packet::string_to_info(packet.get_payload());

        cout << "==============================" << endl;
        file_info.print();
        cout << "==============================" << endl;

        counter++;
    }
}

void Client::end_connection()
{
    char buffer[256];
    recv(sock, buffer, 256, 0);
    if (strcmp(buffer, "exit") == 0)
    {
        close(sock);
    }
}


//TODO: FIZ NOVO
// void Client::startSyncThread() {
//     std::thread sync_thread(&Client::monitor_sync_dir, this);
//     sync_thread.detach();
// }

// void Client::join_sync_thread() {
//     sync_thread.join();
// }

void Client::monitor_sync_dir() {
    create_dir("sync_dir");
    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    std::string sync_dir = exec_path + "/sync_dir";

    int fd = inotify_init();
    if (fd < 0) {
        perror("inotify_init");
        return;
    }

    int wd = inotify_add_watch(fd, sync_dir.c_str(), IN_CREATE | IN_DELETE | IN_MODIFY);
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
                if (event->mask & IN_CREATE) {
                    // std::cout << "Arquivo criado: " << event->name << std::endl;
                    send_cmd("upload");
                    send_file(sync_dir + "/" + std::string(event->name));

            }
                // if (event->mask & IN_DELETE) {
                //     // std::cout << "Arquivo excluído: " << event->name << std::endl;
                //     send_cmd("delete");
                //     send_file_name(event->name);
                // }
                // if (event->mask & IN_MODIFY) {
                //     // std::cout << "Arquivo modificado: " << event->name << std::endl;
                //     send_cmd("upload");
                //     send_file(sync_dir + "/" + std::string(event->name));
                // }
            }

            i += sizeof(struct inotify_event) + event->len;
        }
    }

    delete[] buffer;
    inotify_rm_watch(fd, wd);
    close(fd);
}