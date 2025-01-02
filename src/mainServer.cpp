#include "serverClass.h"
#include <iostream>
#include <thread>

int main(int argc, char const *argv[])
{
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0] << " <port>" << "type -p/-b" << std::endl;
        return 1;
    }

    std::string type = argv[2];
    
    int port = std::stoi(argv[1]);
    Server server(port, type); // Cria um servidor na porta especificada pelo usuário

    if (type == "-p")
    {
        if (!server.start())
        {
            return 1;
        }

        // Cria uma thread para aceitar clientes de forma contínua
        std::thread accept_thread(&Server::acceptClients, &server);
        // Espera até que o servidor seja encerrado
        std::cout << "Pressione Enter para encerrar o servidor..." << std::endl;
        std::cin.get();

        server.stop();

        // Aguarda que a thread de aceitação de clientes termine
        if (accept_thread.joinable())
        {
            accept_thread.join();
        }
        std::cout << "Servidor encerrado com sucesso." << std::endl;
    }
    else if (type == "-b")
    {
        string main_ip_address = argv[3];
        string main_port = argv[4];
        server.backup_name = argv[5];
        std::cout << "Connecting to main server: " << main_ip_address << " | port: " << main_port << std::endl;

        std::thread server_thread([&server, main_ip_address, main_port]()
        {
            server.connect_server(main_ip_address, main_port);
        });
        server_thread.join();
    }

    return 0;
}
