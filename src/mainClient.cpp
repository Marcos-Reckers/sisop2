#include "clientClass.h"

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
    if (server == NULL)
    {
        std::cerr << "ERROR, no such host" << std::endl;
        exit(0);
    }
    // cria a comunication_thread
    Client client(args[1], server, args[3]);
    std::thread communication_thread(Client::handle_connection, &client);

    communication_thread.join();
    return 0;    
}