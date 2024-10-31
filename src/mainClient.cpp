#include "clientClass.h"
#include <iostream>
#include <unistd.h>
#include <netdb.h>

int main(int argc, char const *argv[])
{
    if (argc < 4)
    {
        std::cerr << "Uso: " << argv[0] << "<username> <hostname> <port>" << std::endl;
        return 1;
    }
    
    std::vector<std::string> args{argv, argv + argc};

    struct hostent *server;

    server = gethostbyname(argv[2]);
    if (server == NULL) {
        std::cerr << "ERROR, no such host" << std::endl;
        exit(0);
    }

    Client client(args[1], server, args[3]);

    int sock = client.connect_to_server();
    if (sock < 0)
    {
        std::cerr << "Erro ao conectar ao servidor" << std::endl;
        std::cerr << "Erro: " << sock << std::endl;
        return 1;
    }

    while (true)
    {
        std::string cmd;
        std::cout << "Digite um comando: ";
        std::getline(std::cin, cmd);

        if (cmd.rfind("upload", 0) == 0)
        {
            std::string file_path = cmd.substr(7);
            if (!file_path.empty())
            {
                client.send_cmd("upload");
                client.send_file(file_path);
            }
            else
            {
                std::cerr << "Comando inválido. Uso: upload <path/filename.ext>" << std::endl;
            }
        }

        if (cmd.rfind("delete", 0) == 0)
        {
            std::string file_name = cmd.substr(7);
            if (!file_name.empty())
            {
                client.send_cmd("delete");
                client.send_file_name(file_name);
            }
            else
            {
                std::cerr << "Comando inválido. Uso: delete <filename.ext>" << std::endl;
            }
        }

        if (cmd.rfind("download", 0) == 0)
        {
            std::string file_name = cmd.substr(9);
            if (!file_name.empty())
            {
                client.send_cmd("download");
                client.send_file_name(file_name);
                client.receive_file();
            }
            else
            {
                std::cerr << "Comando inválido. Uso: download <filename.ext>" << std::endl;
            }
        }

        if (cmd.rfind("list_server", 0) == 0)
        {
            client.send_cmd("list_server");
        }

        if (cmd.rfind("list_client", 0) == 0)
        {
            client.send_cmd("list_client");
        }

        if (cmd.rfind("exit", 0) == 0)
        {
            client.send_cmd("exit");
            client.end_connection();
            return 0;
        }
    }

    close(sock);

    return 0;
}