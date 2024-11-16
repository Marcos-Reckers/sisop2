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

    public:

        Client(string username, struct hostent* server, string server_port);

        int16_t connect_to_server();
 
        bool end_connection();
        void get_sync_dir();
        void handle_sync(int sock);

        //void monitor_sync_dir();

        // void handle_upload_request(string file_path, int client_sock);
        // void handle_download_request();
        // void handle_list_server_request();
        // void handle_list_client_request();

        void handle_download_request();
        void handle_upload_request();
        void handle_delete_request();

        void set_sock(int sock);
};


#endif // CLIENTCLASS_H
