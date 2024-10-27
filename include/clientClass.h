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
        string server_ip_address;
        string server_port;

        uint16_t sock;

    public:

        Client(string username, string server_ip_address, string server_port);

        uint16_t connect_to_server();

        void send_cmd(string cmd);
        void send_file(string file_path);

        //upload
        //download
        //delete
        //list_server
        //list_client
        //get_sync_dir

        //exit -> last?? handshake : client sends fin -> server sends ack-fin -> client sends ack back

        void set_sock(uint16_t sock);
};


#endif

// controller class
// send_file()
// receive_file()
