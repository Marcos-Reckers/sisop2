#ifndef CLIENTCLASS_H
#define CLIENTCLASS_H

#include "fileInfo.h"
#include <set>
#include <mutex>

using namespace std;

class Client
{
private:
    string username;
    struct hostent *server;
    string server_port;
    int sock;
    

public:
    set<string> synced_files;
    Client(string username, struct hostent *server, string server_port);
    void set_sock(int sock);

    void handle_connection();
    void send_commands(Threads::AtomicQueue<std::vector<Packet>> &send_queue, Threads::AtomicQueue<std::vector<Packet>> &received_queue);
    int16_t connect_to_server();
    void get_sync_dir(Threads::AtomicQueue<std::vector<Packet>> &send_queue, Threads::AtomicQueue<std::vector<Packet>> &received_queue);

    void handle_sync(Threads::AtomicQueue<std::vector<Packet>> &sync_queue, string folder_name, set<string> &synced_files);
    
    void monitor_sync_dir(string folder_name, Threads::AtomicQueue<std::vector<Packet>> &send_queue, set<string> &synced_files);
    void handle_io(Threads::AtomicQueue<std::vector<Packet>> &send_queue, Threads::AtomicQueue<std::vector<Packet>> &received_queue, Threads::AtomicQueue<std::vector<Packet>> &sync_queue);
};

#endif // CLIENTCLASS_H
