#include "serverClass.h"
#include <iostream>
#include <thread>

int main(int argc, char const *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return 1;
    }
    int port = std::stoi(argv[1]);
    Server server(port);  // Cria um servidor na porta especificada pelo usuário

    if (!server.start()) {
        return 1;
    }

    // Cria uma thread para aceitar clientes de forma contínua
    std::thread accept_thread(&Server::acceptClients, &server);

    // Espera até que o servidor seja encerrado
    std::cout << "Pressione Enter para encerrar o servidor..." << std::endl;
    std::cin.get();

    server.stop();

    // Aguarda que a thread de aceitação de clientes termine
    if (accept_thread.joinable()) {
        accept_thread.join();
    }

    std::cout << "Servidor encerrado com sucesso." << std::endl;
    return 0;
}
