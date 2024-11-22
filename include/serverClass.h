#ifndef SERVERCLASS_H
#define SERVERCLASS_H

#include "fileInfo.h"

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

    void handle_communication(int client_sock);
    void create_sync_dir(int client_sock);

    static void handle_commands(int &client_sock, std::string folder_name, Threads::AtomicQueue<std::vector<Packet>> &send_queue, Threads::AtomicQueue<std::vector<Packet>> &received_queue);
    static void handle_sync(int &client_sock, std::string folder_name, Threads::AtomicQueue<std::vector<Packet>> &sync_queue);
    static void monitor_sync_dir(int &client_sock, string folder_name, Threads::AtomicQueue<std::vector<Packet>> &send_queue);
    static void handle_io(int &client_sock, Threads::AtomicQueue<std::vector<Packet>> &send_queue, Threads::AtomicQueue<std::vector<Packet>> &received_queue, Threads::AtomicQueue<std::vector<Packet>> &sync_queue);

    // Método para adicionar uma thread de cliente ao vetor de threads
    void addClientThread(std::thread &&thread);



    // Método para encerrar a conexão com um cliente
    void close_connection(int client_sock);




    // Getters
    int getServerFd() const;
    sockaddr_in &getServerAddr();
    socklen_t getAddrLen() const;
    std::vector<std::thread> &getClientThreads();
    std::map<int, std::string> &getClients();
    std::string getUsername(int client_sock);
};

#endif // SERVERCLASS_H
