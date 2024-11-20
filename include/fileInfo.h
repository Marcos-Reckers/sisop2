#ifndef FILEINFO_H
#define FILEINFO_H

#include <string>
#include <sys/stat.h>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <sys/socket.h>
#include <fstream>
#include <sys/inotify.h>
#include <limits.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <netdb.h>
#include <thread>
#include <map>
#include <semaphore.h>
#include <netinet/in.h>
#include <vector>
#include <cstdint>
#include <cmath>
#include <sstream>
#include <queue>
#include <mutex>
#include <optional>
#include <fcntl.h>
#include "packet.h"

namespace Threads
{
    template <typename T>
    class AtomicQueue
    {
    private:
        std::mutex lock = std::mutex();
        std::queue<T> resources;

    public:
        AtomicQueue() : resources(std::queue<T>()) {}
        AtomicQueue(std::queue<T> resources) : resources(resources) {}

        void produce(T resource)
        {
            const std::lock_guard<std::mutex> lock_guard(lock);
            resources.push(resource);
        }

        std::optional<T> consume()
        {
            const std::lock_guard<std::mutex> lock_guard(lock);
            if (resources.empty())
            {
                return std::nullopt;
            }

            auto resource = resources.front();
            resources.pop();
            return resource;
        }

        T consume_blocking()
        {
            auto resource = this->consume();
            while (!resource.has_value())
            {
                resource = this->consume();
            }
            return resource.value();
        }
    };
} // namespace Threads (usando código do IAN KERSZ AMARAL)

using namespace std;

class FileInfo
{
private:
    string file_name;
    int file_size;
    string m_time;
    string a_time;
    string c_time;

public:
    FileInfo();
    FileInfo(string file_name, int file_size, string m_time, string a_time, string c_time);

    void retrieve_info_from_file(string path);
    void print();

    string get_file_name() { return file_name; }
    int get_file_size() { return file_size; }
    string get_m_time() { return m_time; }
    string get_a_time() { return a_time; }
    string get_c_time() { return c_time; }

    void set_file_name(string file_name) { this->file_name = file_name; }
    void set_file_size(int file_size) { this->file_size = file_size; }
    void set_m_time(string m_time) { this->m_time = m_time; }
    void set_a_time(string a_time) { this->a_time = a_time; }
    void set_c_time(string c_time) { this->c_time = c_time; }

    vector<Packet> static create_packet_vector(string command, string file_path_or_file_name = "");

    static int most_recent_time(std::string time1, std::string time2);

    void static send_file(string file_path, int client_sock);
    void static send_file_info(int client_sock, std::string file_path);
    void static create_dir(string dir_name);

    vector<FileInfo> static list_files(string path);
    void static send_list_files(vector<FileInfo> files, int socket);
    void static print_list_files(vector<FileInfo> files);
    void static delete_file(string file_path);
    void static send_file_name(string file_path, int sock);
    void static send_cmd(std::string cmd, int sock);


    static string receive_file(Threads::AtomicQueue<std::vector<Packet>> &received_queue, string dst_folder);
    static FileInfo receive_file_info(vector<Packet> &received_packet);
    static vector<FileInfo> receive_list_server(Threads::AtomicQueue<std::vector<Packet>> &received_queue);
    

    static ssize_t sendAll(int sockfd, const void *buf, size_t len, int flags);
    static ssize_t recvAll(int sockfd, void *buf, size_t len, int flags);
};

#endif // FILEINFO_H
