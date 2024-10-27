#include "clientClass.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <filesystem>  

Client::Client(string username, string server_ip_address, string server_port) : 
username(username), server_ip_address(server_ip_address), server_port(server_port) {}

uint16_t Client::connect_to_server() 
{
    struct sockaddr_in serv_addr;

    // Cria o socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        cout << "Erro ao criar socket" << endl;
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(server_port.c_str()));

    // Converte o endereço IP para binário
    if (inet_pton(AF_INET, server_ip_address.c_str(), &serv_addr.sin_addr) <= 0) {
        cout << "Endereço inválido ou não suportado" << endl;
        return -2;
    }

    // Conecta ao servidor -> Faz o handshake
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        cout << "Falha na conexão" << endl;
        close(sock);
        return -3;
    }

    set_sock(sock);

    return sock;

}

void Client::send_cmd(string cmd)
{

    Packet packet = Packet::create_packet_cmd(cmd);
    std::vector<uint8_t> packet_bytes = Packet::packet_to_bytes(packet);
    ssize_t sent_bytes = send(sock, packet_bytes.data(), packet_bytes.size(), 0);
    
    if (sent_bytes < 0) {
        std::cerr << "Erro ao enviar comando como pacote." << std::endl;
        return;
    }

    std::cout << "Comando enviado como pacote: " << cmd << std::endl;
}

void Client::send_file(string file_path)
{
    
    string file_name = std::filesystem::path(file_path).filename().string();
    std::vector<uint8_t> file_name_bytes(file_name.begin(), file_name.end());
    file_name_bytes.push_back('\0');

    ssize_t sent_bytes = send(sock, file_name_bytes.data(), file_name_bytes.size(), 0);
    if (sent_bytes < 0) {
        std::cerr << "Erro ao enviar nome do arquivo." << std::endl;
        return;
    }
    std::cout << "Nome do arquivo enviado: " << file_name << std::endl;
    
    uint32_t file_size = std::filesystem::file_size(file_path);
    uint32_t file_size_network_order = htonl(file_size);
    sent_bytes = send(sock, &file_size_network_order, sizeof(file_size_network_order), 0);
    if (sent_bytes < 0) {
        std::cerr << "Erro ao enviar o tamanho do arquivo." << std::endl;
        return;
    }
    std::cout << "Tamanho do arquivo: " << file_size << " bytes." << std::endl;
    
    std::vector<Packet> packets = Packet::create_packets_from_file(file_path);

    if (packets.empty()){
        cout << "Arquivo vazio." << endl;
        return;
    }

    for (Packet packet : packets) {
        std::vector<uint8_t> packet_bytes = Packet::packet_to_bytes(packet);

        ssize_t sent_bytes = send(sock, packet_bytes.data(), packet_bytes.size(), 0);
        if (sent_bytes < 0) {
            cout << "Erro ao enviar pacote." << endl;
            return;
        }

        std::cout << "Pacote " << packet.get_seqn() << " de tamanho: " << sent_bytes << " bytes." << std::endl;
    }

    std::cout << "Arquivo enviado com sucesso." << std::endl;
}

void Client::set_sock(uint16_t sock) { sock = sock; }
