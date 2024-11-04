#include <iostream>
#include <cstring>      // Para memset
#include <sys/socket.h> // Para sockets
#include <arpa/inet.h>  // Para inet_pton, sockaddr_in
#include <unistd.h>     // Para close
#include <thread>       // Para std::thread
#include <vector>       // Para armazenar threads ativas
#include <string>       // Para std::string
#include <filesystem>
#include <fstream>
#include <map>
#include <dirent.h>
#include <sys/stat.h>

#include "serverClass.h"
#include "packet.h"
#include "fileInfo.h"

// Construtor da classe que recebe a porta como argumento
Server::Server(int port) : server_fd(-1), port(port)
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

    if (!std::filesystem::exists("users"))
    {
        std::filesystem::create_directory("users");
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
        if (client_fd < 0)
        {
            std::cerr << "Erro ao aceitar conexão do cliente." << std::endl;
            continue;
        }
        else
        {
            char username[256] = {0};
            recv(client_fd, username, sizeof(username), 0);
            addClient(client_fd, username);
            std::string username_str(username);
            std::cout << "Conexão aceita de: " << username << std::endl;
            // get_sync_dir(client_fd);
            create_client_dir(username);
        }

        // Cria uma nova thread para lidar com o cliente
        client_threads.emplace_back(&Server::handleRequest, this, client_fd);
    }
}

void Server::get_sync_dir(int client_fd)
{
    create_client_dir(getUsername(client_fd));
}

void Server::create_client_dir(std::string username)
{
    if (!std::filesystem::exists("users/sync_dir_" + username))
    {
        std::filesystem::create_directory("users/sync_dir_" + username);
    }
}

// Método que adiciona o cliente ao map
void Server::addClient(int client_fd, std::string username)
{
    clients[client_fd] = username;
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

void Server::handleRequest(int client_sock)
{
    std::cout << "Entrou em handleRequest, com: " << getUsername(client_sock) << std::endl;

    while (true)
    {
        ssize_t total_size = Packet::packet_base_size() + MAX_PAYLOAD_SIZE;
        std::vector<uint8_t> packet_buffer(total_size);
        ssize_t received_bytes = recv(client_sock, packet_buffer.data(), packet_buffer.size(), 0);

        if (received_bytes < 0)
        {
            std::cerr << "Erro ao receber o pacote." << std::endl;
            return;
        }

        if (received_bytes == 0)
        {
            close(client_sock);
            std::cout << "Conexão encerrada." << std::endl;
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
                receive_file(client_sock);
            }
            if (cmd.substr(0, 8) == "download")
            {
                send_file(client_sock);
            }
            if (cmd.substr(0, 6) == "delete")
            {
                delete_file(client_sock);
            }
            if (cmd.substr(0, 11) == "list_server")
            {
                // list_files_server(client_sock);
            }
            if (cmd.substr(0, 11) == "list_client")
            {
                // list_client(client_fd);
            }
            if (cmd.substr(0, 12) == "get_sync_dir")
            {
                // get_sync_dir(client_fd);
            }
            if (cmd.substr(0, 4) == "exit")
            {
                close_connection(client_sock);
            }
        }
        else
        {
            std::cerr << "Pacote recebido não é do tipo 1." << std::endl;
        }
    }
}

// Metodo para adicionar uma thread de cliente ao vetor de threads
void Server::addClientThread(std::thread &&thread) { client_threads.push_back(std::move(thread)); }

// Getters
int Server::getServerFd() const { return server_fd; }
sockaddr_in &Server::getServerAddr() { return server_addr; }
socklen_t Server::getAddrLen() const { return sizeof(server_addr); }
std::vector<std::thread> &Server::getClientThreads() { return client_threads; }

void Server::receive_file(int client_sock)
{
    char file_name_buffer[256] = {0};
    ssize_t received_bytes = recv(client_sock, file_name_buffer, sizeof(file_name_buffer), 0);
    std::string username = getUsername(client_sock);
    std::string save_path = "users/sync_dir_" + username + "/" + file_name_buffer;
    if (received_bytes <= 0)
    {
        std::cerr << "Erro ao receber o nome do arquivo." << std::endl;
        return;
    }
    std::string file_name(file_name_buffer, received_bytes);
    std::cout << "Nome do arquivo recebido: " << file_name << std::endl;

    uint32_t file_size_network_order;
    received_bytes = recv(client_sock, &file_size_network_order, sizeof(file_size_network_order), 0);
    if (received_bytes <= 0)
    {
        std::cerr << "Erro ao receber o tamanho do arquivo." << std::endl;
        return;
    }

    uint32_t file_size = ntohl(file_size_network_order);
    std::cout << "Tamanho do arquivo recebido: " << file_size << " bytes" << std::endl;

    std::ofstream outfile(save_path, std::ios::binary);
    if (!outfile.is_open())
    {
        std::cerr << "Erro ao abrir o arquivo: " << file_name << std::endl;
        return;
    }

    uint32_t total_received_bytes = 0;
    while (total_received_bytes < file_size)
    {
        ssize_t total_size = Packet::packet_base_size() + MAX_PAYLOAD_SIZE;

        std::vector<uint8_t> packet_buffer(total_size);

        received_bytes = recv(client_sock, packet_buffer.data(), packet_buffer.size(), 0);
        if (received_bytes <= 0)
        {
            std::cerr << "Erro ao receber o pacote:" << std::endl;
            break;
        }
        std::cout << "Bytes recebidos: " << received_bytes << std::endl;

        Packet packet = Packet::bytes_to_packet(packet_buffer);
        outfile.write(packet.get_payload().data(), packet.get_length());

        total_received_bytes += packet.get_length();

        // Debug
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

void Server::delete_file(int client_sock)
{
    char file_name_buffer[256] = {0};
    std::string username = getUsername(client_sock);
    ssize_t received_bytes = recv(client_sock, file_name_buffer, sizeof(file_name_buffer), 0);

    std::string save_path = "users/" + username + "/" + file_name_buffer;

    if (received_bytes <= 0)
    {
        std::cerr << "Erro ao receber o nome do arquivo." << std::endl;
        return;
    }
    std::string file_name(file_name_buffer, received_bytes);
    std::cout << "Nome do arquivo recebido: " << file_name << std::endl;

    if (std::filesystem::remove(save_path))
    {
        std::cout << "Arquivo deletado com sucesso." << std::endl;
    }
    else
    {
        std::cerr << "Erro ao deletar o arquivo." << std::endl;
    }
}

void Server::send_file(int client_sock)
{
    char file_name_buffer[256] = {0};
    std::string username = getUsername(client_sock);
    ssize_t received_bytes = recv(client_sock, file_name_buffer, sizeof(file_name_buffer), 0);

    std::string save_path = "users/sync_dir_" + username + "/" + file_name_buffer;

    if (received_bytes <= 0)
    {
        std::cerr << "Erro ao receber o nome do arquivo." << std::endl;
        return;
    }
    std::string file_name(file_name_buffer, received_bytes);
    std::cout << "Nome do arquivo recebido: " << file_name << std::endl;

    if (!std::filesystem::exists(save_path))
    {
        std::cerr << "Arquivo não encontrado." << std::endl;
        return;
    }

    std::vector<uint8_t> file_name_bytes(file_name.begin(), file_name.end());
    file_name_bytes.push_back('\0');

    // send_package_info(client_sock, save_path);

    send_file_info(client_sock, save_path);

    std::vector<Packet> packets = Packet::create_packets_from_file(save_path);

    if (packets.empty())
    {
        std::cout << "Arquivo vazio." << std::endl;
        return;
    }

    // Envia conteudo do arquivo
    for (Packet packet : packets)
    {
        std::vector<uint8_t> packet_bytes = Packet::packet_to_bytes(packet);

        ssize_t sent_bytes = send(client_sock, packet_bytes.data(), packet_bytes.size(), 0);
        if (sent_bytes < 0)
        {
            std::cout << "Erro ao enviar pacote." << std::endl;
            return;
        }

        std::cout << "Pacote " << packet.get_seqn() << " de tamanho: " << sent_bytes << " bytes." << std::endl;
    }

    std::cout << "Arquivo enviado com sucesso." << std::endl;
}

void Server::close_connection(int client_sock)
{
    send(client_sock, "exit", 4, 0);
}

void Server::send_package_info(int client_sock, std::string file_path)
{
    std::string file_name = std::filesystem::path(file_path).filename().string();
    std::vector<uint8_t> file_name_bytes(file_name.begin(), file_name.end());
    file_name_bytes.push_back('\0');

    // Envia nome do arquivo
    ssize_t sent_bytes = send(client_sock, file_name_bytes.data(), file_name_bytes.size(), 0);
    if (sent_bytes < 0)
    {
        std::cerr << "Erro ao enviar nome do arquivo." << std::endl;
        return;
    }
    std::cout << "Nome do arquivo enviado: " << file_name << std::endl;

    uint32_t file_size = std::filesystem::file_size(file_path);
    uint32_t file_size_network_order = htonl(file_size);
    sent_bytes = send(client_sock, &file_size_network_order, sizeof(file_size_network_order), 0);
    if (sent_bytes < 0)
    {
        std::cerr << "Erro ao enviar o tamanho do arquivo." << std::endl;
        return;
    }
    std::cout << "Tamanho do arquivo: " << file_size << " bytes." << std::endl;
}

void Server::send_file_info(int client_sock, std::string file_path)
{

    FileInfo file_info;
    file_info.retrieve_info_from_file(file_path);
    file_info.print();

    Packet packet;
    packet = packet.create_packet_info(file_info);
    packet.print();

    std::vector<uint8_t> packet_bytes = Packet::packet_to_bytes(packet);
    ssize_t sent_bytes = send(client_sock, packet_bytes.data(), packet_bytes.size(), 0);

    if (sent_bytes < 0)
    {
        std::cerr << "Erro ao enviar informações do arquivo." << std::endl;
    }
}

std::string Server::getUsername(int client_sock)
{
    return clients[client_sock];
}

std::map<int, std::string> &Server::getClients()
{
    return clients;
}
