#ifndef SERVERCLASS_H
#define SERVERCLASS_H

#include <string>
#include <vector>
#include <thread>
#include <netinet/in.h> // Para sockaddr_in
#include "packet.h"

class Server {
private:
    int server_fd;                     // File descriptor do socket do servidor
    sockaddr_in server_addr;           // Estrutura de endereço do servidor
    int port;                          // Porta na qual o servidor escuta
    std::vector<std::thread> client_threads; // Vetor para armazenar threads de clientes


public:
    // Construtor que inicializa o servidor com uma porta específica
    explicit Server(int port);

    // Método para iniciar o servidor (cria o socket, faz o bind e coloca o servidor em escuta)
    bool start();

    // Método para aceitar conexões de clientes e iniciar uma nova thread para cada um
    void acceptClients();

    // Método para encerrar o servidor e todas as conexões ativas de clientes
    void stop();

    // Método para adicionar uma thread de cliente ao vetor de threads
    void addClientThread(std::thread&& thread);

    // Getters
    int getServerFd() const;
    sockaddr_in& getServerAddr();
    socklen_t getAddrLen() const;
    std::vector<std::thread>& getClientThreads();

    void handleRequest(int client_fd);

    void receive_file(int client_fd);
    void close_connection(int client_fd);


};

#endif // SERVERCLASS_H
