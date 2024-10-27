#include <iostream>
#include <cstring>      // Para memset
#include <sys/socket.h> // Para sockets
#include <arpa/inet.h>  // Para inet_pton, sockaddr_in
#include <unistd.h>     // Para close
#include <thread>       // Para std::thread
#include <vector>       // Para armazenar threads ativas
#include <string>       // Para std::string
#include "serverClass.h"

#include <filesystem>  
#include <fstream>
#include <vector>
#include "packet.h"

// Construtor da classe que recebe a porta como argumento
Server::Server(int port) : server_fd(-1), port(port) {
    memset(&server_addr, 0, sizeof(server_addr));
}

// Método para iniciar o servidor
bool Server::start() {
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "Erro ao criar o socket." << std::endl;
        return false;
    }

    // Configuração do endereço do servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Escuta em qualquer interface
    server_addr.sin_port = htons(port);

    // Bind do socket à porta
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Erro ao fazer o bind na porta " << port << "." << std::endl;
        close(server_fd);
        return false;
    }

    // Coloca o servidor em modo de escuta
    if (listen(server_fd, 10) < 0) { // 10 clientes na fila de espera
        std::cerr << "Erro ao colocar o servidor em modo de escuta." << std::endl;
        close(server_fd);
        return false;
    }

    std::cout << "Servidor iniciado e aguardando conexões na porta " << port << "." << std::endl;
    return true;
}

// Método para aceitar conexões de clientes em novas threads
void Server::acceptClients() {
    while (true) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            std::cerr << "Erro ao aceitar conexão do cliente." << std::endl;
            continue;
        }

        std::cout << "Conexão aceita de " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << std::endl;

        // Cria uma nova thread para lidar com o cliente e armazena a thread no vetor
        client_threads.emplace_back(&Server::handleRequest, this, client_fd);
    }
}

// Método para encerrar o servidor
void Server::stop() {
    if (server_fd != -1) {
        close(server_fd);
        server_fd = -1;
        std::cout << "Servidor encerrado." << std::endl;
    }

    // Espera que todas as threads dos clientes terminem
    for (auto& thread : client_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void Server::handleRequest(int client_fd)
{
    std::cout << "Entrou em handleRequest, com o client_id: " << client_fd << std::endl;

    std::vector<uint8_t> packet_buffer(MAX_PAYLOAD_SIZE + sizeof(Packet));
    ssize_t received_bytes = recv(client_fd, packet_buffer.data(), packet_buffer.size(), 0);
    if (received_bytes <= 0) {
        std::cerr << "Erro ao receber o pacote." << std::endl;
        return;
    }
    Packet pkt = Packet::bytes_to_packet(packet_buffer);
    pkt.print();

    if (pkt.get_type() == 1) {
        std::vector<char> cmd_vec = pkt.get_payload(); 
        std::string cmd(cmd_vec.begin(), cmd_vec.end());
        std::cout << "Comando recebido: " << cmd << std::endl;
        if(cmd.substr(0, 6) == "upload") {
            receive_file(client_fd);
        }
        if(cmd.substr(0, 8) == "download") {
            //send_file(client_fd);
        }
        if(cmd.substr(0, 6) == "delete") {
            //delete_file(client_fd);
        }
        if(cmd.substr(0, 11) == "list_server") {
            //list_server(client_fd);
        }
        if(cmd.substr(0, 11) == "list_client") {
            //list_client(client_fd);
        }
        if(cmd.substr(0, 12) == "get_sync_dir") {
            //get_sync_dir(client_fd);
        }
        if(cmd.substr(0, 4) == "exit") {
            close_connection(client_fd);
        }        
    }
    else{
        std::cerr << "Pacote recebido não é do tipo 1." << std::endl;
    } 
}

// Metodo para adicionar uma thread de cliente ao vetor de threads
void Server::addClientThread(std::thread&& thread) { client_threads.push_back(std::move(thread)); }

// Getters
int Server::getServerFd() const { return server_fd; }
sockaddr_in& Server::getServerAddr() { return server_addr; }
socklen_t Server::getAddrLen() const { return sizeof(server_addr); }
std::vector<std::thread>& Server::getClientThreads() { return client_threads; }

void Server::receive_file(int client_sock)
{
    char file_name_buffer[256];
    ssize_t received_bytes = recv(client_sock, file_name_buffer, sizeof(file_name_buffer), 0);
    if (received_bytes <= 0) {
        std::cerr << "Erro ao receber o nome do arquivo." << std::endl;
        return;
    }
    std::string file_name(file_name_buffer);  
    std::cout << "Nome do arquivo recebido: " << file_name << std::endl;

    uint32_t file_size_network_order;
    received_bytes = recv(client_sock, &file_size_network_order, sizeof(file_size_network_order), 0);
    if (received_bytes <= 0) {
        std::cerr << "Erro ao receber o tamanho do arquivo." << std::endl;
        return;
    }
    uint32_t file_size = ntohl(file_size_network_order);  
    std::cout << "Tamanho do arquivo recebido: " << file_size << " bytes" << std::endl;

    std::ofstream outfile(file_name, std::ios::binary);
    if (!outfile.is_open()) {
        std::cerr << "Erro ao abrir o arquivo: " << file_name << std::endl;
        return;
    }

    uint32_t total_received_bytes = 0;
    while (total_received_bytes < file_size) {
        std::vector<uint8_t> packet_buffer(MAX_PAYLOAD_SIZE + sizeof(Packet));
        
        std::cout << "Tamanho do pacote MAX_PAYLOAD_SIZE + SIZEOF(PACKET): " << packet_buffer.size() << std::endl;
        std::cout << "Tamanho de sizeof(Packet): " << sizeof(Packet) << std::endl;

        received_bytes = recv(client_sock, packet_buffer.data(), packet_buffer.size(), 0);
        if (received_bytes <= 0) {
            std::cerr << "Erro ao receber o pacote:" << std::endl;
            break;
        }
        std::cout << "Bytes recebidos: " << received_bytes <<  std::endl;

        Packet packet = Packet::bytes_to_packet(packet_buffer);
        outfile.write(packet.get_payload().data(), packet.get_length());

        total_received_bytes += packet.get_length();

        // Debug
        std::cout << "Pacote " << packet.get_seqn() << " recebido com payload de tamanho: " << packet.get_length() << std::endl;
    }

    outfile.close();

    if (total_received_bytes == file_size) {
        std::cout << "Arquivo recebido com sucesso." << std::endl;
    } else {
        std::cerr << "Transferência de arquivo incompleta. Esperando " << file_size << " bytes mas recebeu " << total_received_bytes << " bytes." << std::endl;
    }
}

void Server::close_connection(int client_sock)
{
    close(client_sock);
    std::cout << "Conexão encerrada." << std::endl;
}