#ifndef CLIENTCLASS_H
#define CLIENTCLASS_H

#include "fileInfo.h"
#include <set>


using namespace std;

class Client
{
    private:
        string username;
        struct hostent* server;
        string server_port;
        set<string> synced_files;

        int sock;

    public:

        Client(string username, struct hostent* server, string server_port);

        int16_t connect_to_server();
 
        bool end_connection();
        void get_sync_dir();
        void handle_sync();

        void handle_download_request();
        void handle_upload_request();
        void handle_delete_request();
        void monitor_sync_dir(string folder);

        void set_sock(int sock);
};


#endif // CLIENTCLASS_H
