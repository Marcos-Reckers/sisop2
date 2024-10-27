#include "clientClass.h"
#include <iostream>
#include <unistd.h>


int main(int argc, char const *argv[])
{
    std::vector<std::string> args{argv, argv + argc};
    Client client(args[1], args[2], args[3]);

    uint8_t socket = client.connect_to_server();
    if (socket < 0) {
        std::cerr << "Erro ao conectar ao servidor" << std::endl;
        std::cerr << "Erro: " << socket << std::endl;
        return 1;
    }
    
    while (true) {
        std::string cmd;
        std::cout << "Digite um comando: ";
        std::getline(std::cin, cmd);
        client.send_cmd(cmd);

        if (cmd.rfind("upload", 0) == 0) {
            std::string file_path = cmd.substr(7);
            if (!file_path.empty()) {
                client.send_file(file_path);
            } else {
                std::cerr << "Comando inválido. Uso: upload <path/filename.ext>" << std::endl;
            }
        }
    }    
   
    // Fecha o socket
    close(socket);

    return 0;
}