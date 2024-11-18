#ifndef CLIENTCLASS_H
#define CLIENTCLASS_H

#include "fileInfo.h"


using namespace std;

class Client
{
    private:
        string username;
        struct hostent* server;
        string server_port;

        int sock;

        // Mapa para rastrear arquivos recebidos e sua origem (S=servidor, C=cliente)
        std::map<std::string, char> received_files;
        std::chrono::steady_clock::time_point last_sync;
        std::chrono::steady_clock::time_point last_upload; // Adicionar essa linha

    public:

        Client(string username, struct hostent* server, string server_port);

        int16_t connect_to_server();
 
        bool end_connection();
        void get_sync_dir();
        void handle_sync(int sock);
        void monitor_sync_dir(std::string folder, int sock);

        void handle_upload_request();
        void handle_delete_request();
        void handle_sync_request(int sock);

        void set_sock(int sock);
};


#endif // CLIENTCLASS_H
