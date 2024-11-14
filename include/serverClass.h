#ifndef SERVERCLASS_H
#define SERVERCLASS_H

#include <string>
#include <vector>
#include <map>
#include <thread>
#include <semaphore.h>
#include <netinet/in.h> // Para sockaddr_in
#include "packet.h"

class Server
{
private:
    int server_fd;                           // File descriptor do socket do servidor
    sockaddr_in server_addr;                 // Estrutura de endereço do servidor
    int port;                                // Porta na qual o servidor escuta
    std::vector<std::thread> client_threads; // Vetor para armazenar threads de clientes
    std::map<int, std::string> clients;      // Map para armazenar os clientes conectados
    std::map<std::string, std::unique_ptr<sem_t>> active;

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
    void receive_file(int client_sock);

    void send_file(int client_sock);

    bool verify_active_sessions(string username);

    // Método para deletar um arquivo
    void delete_file(int client_sock);

    // Método para encerrar a conexão com um cliente
    void close_connection(int client_sock);
    void create_client_dir(std::string username);
    void get_sync_dir(int client_sock);

    void send_package_info(int client_sock, std::string file_path);
    void send_file_info(int client_sock, std::string file_path);

    void list_files_server(int client_sock);

    void sync_client_dir(int client_fd);

    // Getters
    int getServerFd() const;
    sockaddr_in &getServerAddr();
    socklen_t getAddrLen() const;
    std::vector<std::thread> &getClientThreads();
    std::map<int, std::string> &getClients();
    std::string getUsername(int client_sock);
};

#endif // SERVERCLASS_H
