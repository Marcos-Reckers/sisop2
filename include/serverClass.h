#ifndef SERVERCLASS_H
#define SERVERCLASS_H

#include "fileInfo.h"
#include <mutex>
#include <chrono>

class Server
{
private:
    int server_fd;                           // File descriptor do socket do servidor
    sockaddr_in server_addr;                 // Estrutura de endereço do servidor
    int port;                                // Porta na qual o servidor escuta
    std::vector<std::thread> client_threads; // Vetor para armazenar threads de clientes
    std::map<int, std::string> clients;      // Map para armazenar os clientes conectados
    std::map<std::string, std::unique_ptr<sem_t>> active;
    std::map<std::string, std::vector<int>> user_sessions; // Mapa de usuário para seus sockets ativos
    std::mutex broadcast_mutex;              // Adiciona mutex como membro da classe

    public :
    // Construtor que inicializa o servidor com uma porta específica
    explicit Server(int port);

    // Método para iniciar o servidor (cria o socket, faz o bind e coloca o servidor em escuta)
    bool start();

    // Método para aceitar conexões de clientes e iniciar uma nova thread para cada um
    void acceptClients();

    // Método para adicionar um cliente ao map de clientes
    void addClient(int client_sock, std::string username);
    void removeClient(int client_fd);

    // Método para encerrar o servidor e todas as conexões ativas de clientes
    void stop();

    // Método para adicionar uma thread de cliente ao vetor de threads
    void addClientThread(std::thread &&thread);

    // Método para lidar com as requisições de um cliente
    void handleRequest(int client_sock);

    // Método para receber um arquivo

    void handle_download_request(int client_sock);
    void handle_upload_request(int client_sock);
    void handle_list_request(int client_sock);
    void handle_delete_request(int client_sock);

    // Método para encerrar a conexão com um cliente
    void close_connection(int client_sock);
    void get_sync_dir(int client_sock);
    void handle_sync(int client_sock);
    void monitor_sync_dir(std::string folder, int origin_sock);
    void broadcast_to_user(std::string username, std::string file_name, std::string command, int origin_sock);

    // Getters
    int getServerFd() const;
    sockaddr_in &getServerAddr();
    socklen_t getAddrLen() const;
    std::vector<std::thread> &getClientThreads();
    std::map<int, std::string> &getClients();
    std::string getUsername(int client_sock);
};

#endif // SERVERCLASS_H
