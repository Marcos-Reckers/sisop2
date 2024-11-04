#include "clientClass.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <netdb.h>

#include "packet.h"
#include "fileInfo.h"
#include <filesystem>

Client::Client(string username, struct hostent *server, string server_port) : username(username), server(server), server_port(server_port) {}

void Client::set_sock(int sock) { sock = sock; }

uint16_t Client::connect_to_server()
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

    // Conecta ao servidor -> Faz o handshake
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        cout << "Falha na conexão" << endl;
        close(sock);
        return -3;
    }

    send(sock, username.c_str(), username.size(), 0);

    set_sock(sock);

    return sock;
}

void Client::send_cmd(string cmd)
{

    Packet packet = Packet::create_packet_cmd(cmd);
    std::vector<uint8_t> packet_bytes = Packet::packet_to_bytes(packet);
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

        std::cout << "Pacote " << packet.get_seqn() << " de tamanho: " << sent_bytes << " bytes." << std::endl;
    }

    std::cout << "Arquivo enviado com sucesso." << std::endl;
}

/* void Client::create_download_dir(){
    if (!std::filesystem::exists("downloads")) {
    std::filesystem::create_directory("downloads");
    }
} */

void Client::create_dir(string dir_name)
{
    if (!std::filesystem::exists(dir_name))
    {
        std::filesystem::create_directory(dir_name);
    }
}

void Client::get_sync_dir()
{
    create_dir("sync_dir");
    send_cmd("get_sync_dir");
}

void Client::receive_file()
{
    // char file_name_buffer[256] = {0};
    // ssize_t received_bytes = recv(sock, file_name_buffer, sizeof(file_name_buffer), 0);

    // //create_download_dir();
    // create_dir("downloads");

    // std::string save_path = "downloads/" + std::string(file_name_buffer);
    // if (received_bytes <= 0)
    // {
    //     std::cerr << "Erro ao receber o nome do arquivo." << std::endl;
    //     return;
    // }
    // std::string file_name(file_name_buffer, received_bytes);
    // std::cout << "Nome do arquivo recebido: " << file_name << std::endl;

    // uint32_t file_size_network_order;
    // received_bytes = recv(sock, &file_size_network_order, sizeof(file_size_network_order), 0);
    // if (received_bytes <= 0)
    // {
    //     std::cerr << "Erro ao receber o tamanho do arquivo." << std::endl;
    //     return;
    // }
    // uint32_t file_size = ntohl(file_size_network_order);
    // std::cout << "Tamanho do arquivo recebido: " << file_size << " bytes" << std::endl;

    // std::ofstream outfile(save_path, std::ios::binary);
    // if (!outfile.is_open())
    // {
    //     std::cerr << "Erro ao abrir o arquivo: " << file_name << std::endl;
    //     return;
    // }

    std::cout << "Recebendo informações do arquivo ANTES DE RECEIVE_FILE_INFO" << std::endl;

    FileInfo file_info = receive_file_info();

    std::cout << "Recebi informações do arquivo DEPOIS DE RECEIVE_FILE_INFO" << std::endl;

    string file_name = file_info.get_file_name();
    std::cout << "Nome do arquivo recebido: " << file_name << std::endl;

    int file_size = file_info.get_file_size();
    std::cout << "Tamanho do arquivo recebido: " << file_size << " bytes" << std::endl;

    create_dir("downloads");
    std::string save_path = "downloads/" + file_name;

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
        std::cout << "Bytes recebidos: " << received_bytes << std::endl;

        Packet packet = Packet::bytes_to_packet(packet_buffer);
        outfile.write(packet.get_payload().data(), packet.get_length());

        total_received_bytes += packet.get_length();

        std::cout << "Pacote " << packet.get_seqn() << " recebido com payload de tamanho: " << packet.get_length() << std::endl;
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

    std::vector<uint8_t> packet_buffer(Packet::packet_base_size());
    ssize_t received_bytes = recv(sock, packet_buffer.data(), packet_buffer.size(), 0);
    if (received_bytes <= 0)
    {
        std::cerr << "Erro ao receber o pacote:" << std::endl;
        return FileInfo();
    }

    Packet packet = Packet::bytes_to_packet(packet_buffer);
    packet.print();

    FileInfo file_info = Packet::string_to_info(packet.get_payload());
    std::cout << "Informações do arquivo recebidas em receive_file_info:" << std::endl;

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

void Client::end_connection()
{
    char buffer[256];
    recv(sock, buffer, 256, 0);
    if (strcmp(buffer, "exit") == 0)
    {
        close(sock);
    }
}
