#ifndef CLIENTCLASS_H
#define CLIENTCLASS_H

#include <iostream>
#include <string>
#include "packet.h"

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

        uint16_t connect_to_server();

        void create_download_dir();

        void send_cmd(string cmd);
        void send_file(string file_path);
        void send_file_name(string file_path);
        void receive_file();
        FileInfo receive_file_info();

        void end_connection();
        void create_dir(string dir_name);
        void get_sync_dir();


        //download
        //list_server
        //list_client
        //get_sync_dir

        //exit -> last?? handshake : client sends fin -> server sends ack-fin -> client sends ack back

        void set_sock(int sock);
};


#endif
