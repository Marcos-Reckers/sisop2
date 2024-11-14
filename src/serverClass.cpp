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
#include <semaphore>

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

    // Obtém o caminho do diretório dos executáveis
    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();

    if (!std::filesystem::exists(exec_path + "/users"))
    {
        std::filesystem::create_directory(exec_path + "/users");
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

            // SEMAFORO PARA

            if (!verify_active_sessions(username)){
                close(client_fd);
                continue;
            }
            addClient(client_fd, username);

            std::string username_str(username);
            std::cout << "Conexão aceita de: " << username << std::endl;
        }

        // Cria uma nova thread para lidar com o cliente
        client_threads.emplace_back(&Server::handleRequest, this, client_fd);
    }
}

bool Server::verify_active_sessions(string username){
    std::cout << "Verificando sessões ativas" << std::endl;

    int session_count = 0;
    for (const auto& [session_id, user] : clients) {
        if (user == username) {
            session_count++;
        }
    }

    if (session_count >= 2) {
        std::cout << "O usuário " << username << " já possui duas sessões ativas. Conexão não permitida." << std::endl;
        return false;
    }

    return true;
}

void Server::get_sync_dir(int client_fd)
{
    create_client_dir(getUsername(client_fd));
}

void Server::create_client_dir(std::string username)
{
    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    if (!std::filesystem::exists(exec_path + "/users/sync_dir_" + username))
    {
        std::filesystem::create_directory(exec_path + "/users/sync_dir_" + username);
    }
}

// Método que adiciona o cliente ao map
void Server::addClient(int client_fd, std::string username)
{
    clients[client_fd] = username;
    // client_sockets.insert(username, client_fd);
}

void Server::removeClient(int client_fd)
{
    clients.erase(client_fd);
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
            
            // SEMAFORO VAI

            std::cout << "Conexão do cliente " << getUsername(client_sock) << " encerrada." << std::endl;
            removeClient(client_sock);
            close(client_sock);

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
                list_files_server(client_sock);
            }
            if (cmd.substr(0, 12) == "get_sync_dir")
            {
                get_sync_dir(client_sock);
                // sync_client_dir(client_sock);
                cout << "get_sync_dir criado e sincronização implementada" << endl;
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
    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    std::string save_path = exec_path + "/users/sync_dir_" + username + "/" + file_name_buffer;
    if (received_bytes <= 0)
    {
        std::cerr << "Erro ao receber o nome do arquivo." << std::endl;
        return;
    }
    std::string file_name(file_name_buffer, received_bytes);

    uint32_t file_size_network_order;
    received_bytes = recv(client_sock, &file_size_network_order, sizeof(file_size_network_order), 0);
    if (received_bytes <= 0)
    {
        std::cerr << "Erro ao receber o tamanho do arquivo." << std::endl;
        return;
    }

    uint32_t file_size = ntohl(file_size_network_order);

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

        Packet packet = Packet::bytes_to_packet(packet_buffer);
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

void Server::delete_file(int client_sock)
{
    char file_name_buffer[256] = {0};
    std::string username = getUsername(client_sock);
    ssize_t received_bytes = recv(client_sock, file_name_buffer, sizeof(file_name_buffer), 0);

    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    std::string save_path = exec_path + "/users/sync_dir_" + username + "/" + file_name_buffer;

    if (received_bytes <= 0)
    {
        std::cerr << "Erro ao receber o nome do arquivo." << std::endl;
        return;
    }
    std::string file_name(file_name_buffer, received_bytes);

    if (!std::filesystem::exists(save_path))
    {
        std::cerr << "Erro ao deletar o arquivo." << std::endl;
        std::cerr << "Arquivo não encontrado." << std::endl;
        std::string error_msg = "ERROR: Arquivo não encontrado.";
        Packet error_packet = Packet::create_packet_cmd(error_msg);
        std::vector<uint8_t> error_packet_bytes = Packet::packet_to_bytes(error_packet);
        send(client_sock, error_packet_bytes.data(), error_packet_bytes.size(), 0);
        return;
    }
    else if (std::filesystem::remove(save_path))
    {
        std::cout << "Arquivo deletado com sucesso." << std::endl;
    }
}

void Server::send_file(int client_sock)
{
    char file_name_buffer[256] = {0};
    std::string username = getUsername(client_sock);
    ssize_t received_bytes = recv(client_sock, file_name_buffer, sizeof(file_name_buffer), 0);

    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    std::string save_path = exec_path + "/users/sync_dir_" + username + "/" + file_name_buffer;

    if (received_bytes <= 0)
    {
        std::cerr << "Erro ao receber o nome do arquivo." << std::endl;
        return;
    }
    std::string file_name(file_name_buffer, received_bytes);

    if (!std::filesystem::exists(save_path))
    {
        std::cerr << "Arquivo não encontrado." << std::endl;
        std::string error_msg = "ERROR: Arquivo não encontrado.";
        Packet error_packet = Packet::create_packet_cmd(error_msg);
        std::vector<uint8_t> error_packet_bytes = Packet::packet_to_bytes(error_packet);
        send(client_sock, error_packet_bytes.data(), error_packet_bytes.size(), 0);
        return;
    }

    std::vector<uint8_t> file_name_bytes(file_name.begin(), file_name.end());
    file_name_bytes.push_back('\0');

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

        std::cout << "Pacote " << packet.get_seqn() << " de tamanho: " << sent_bytes << " bytes enviado." << std::endl;
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

    uint32_t file_size = std::filesystem::file_size(file_path);
    uint32_t file_size_network_order = htonl(file_size);
    sent_bytes = send(client_sock, &file_size_network_order, sizeof(file_size_network_order), 0);
    if (sent_bytes < 0)
    {
        std::cerr << "Erro ao enviar o tamanho do arquivo." << std::endl;
        return;
    }
}

void Server::send_file_info(int client_sock, std::string file_path)
{
    FileInfo file_info;
    file_info.retrieve_info_from_file(file_path);
    file_info.print();

    Packet packet = Packet::create_packet_info(file_info);

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

void Server::list_files_server(int client_sock)
{
    std::string username = getUsername(client_sock);
    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    std::string path = exec_path + "/users/sync_dir_" + username;

    if (std::filesystem::is_empty(path))
    {
        // std::string empty_folder = "SERVER: empty folder";
        // Packet empty_packet = Packet::create_packet_cmd(empty_folder);
        // std::vector<uint8_t> empty_packet_bytes = Packet::packet_to_bytes(empty_packet);
        // send(client_sock, empty_packet_bytes.data(), empty_packet_bytes.size(), 0);
        cout << "Pasta vazia" << endl;
    }
    else{
        for (const auto &entry : std::filesystem::directory_iterator(path))
        {
            FileInfo file_info;
            file_info.retrieve_info_from_file(entry.path());

            Packet packet = Packet::create_packet_info(file_info);
            std::vector<uint8_t> packet_bytes = Packet::packet_to_bytes(packet);

            ssize_t sent_bytes = send(client_sock, packet_bytes.data(), packet_bytes.size(), 0);
            if (sent_bytes < 0)
            {
                std::cerr << "Erro ao enviar informações do arquivo." << std::endl;
                return;
            }

            std::cout << "Informações do arquivo enviadas: " << entry.path().filename().string() << std::endl;
        }
    }

    // Envia uma string especial para indicar o fim da lista
    std::string end_of_list = "END_OF_LIST";
    Packet end_packet = Packet::create_packet_cmd(end_of_list);
    std::vector<uint8_t> end_packet_bytes = Packet::packet_to_bytes(end_packet);
    send(client_sock, end_packet_bytes.data(), end_packet_bytes.size(), 0);
}

// void Server::sync_client_dir(int client_fd)
// {
//     std::string username = getUsername(client_fd);
//     std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
//     std::string client_dir = exec_path + "/users/sync_dir_" + username;

//     for (const auto &entry : std::filesystem::directory_iterator(client_dir))
//     {
//         send_file_info(client_fd, entry.path().string());
//         std::vector<Packet> packets = Packet::create_packets_from_file(entry.path().string());

//         for (Packet packet : packets)
//         {
//             std::vector<uint8_t> packet_bytes = Packet::packet_to_bytes(packet);
//             ssize_t sent_bytes = send(client_fd, packet_bytes.data(), packet_bytes.size(), 0);
//             if (sent_bytes < 0)
//             {
//                 std::cerr << "Erro ao enviar pacote." << std::endl;
//                 return;
//             }
//         }
//     }

//     std::string end_of_sync = "END_OF_SYNC";
//     Packet end_packet = Packet::create_packet_cmd(end_of_sync);
//     std::vector<uint8_t> end_packet_bytes = Packet::packet_to_bytes(end_packet);
//     send(client_fd, end_packet_bytes.data(), end_packet_bytes.size(), 0);
// }
