#ifndef FILEINFO_H
#define FILEINFO_H

#include <string>
#include "packet.h"

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

    void static send_file(string file_path, int client_sock);
    void static send_file_info(int client_sock, std::string file_path);
    void static receive_file(string directory, int sock);
    void static create_dir(string dir_name);
    FileInfo static receive_file_info(int sock);
    vector<FileInfo> static list_files(string path);
    void static send_list_files(vector<FileInfo> files, int socket);
    void static print_list_files(vector<FileInfo> files);
    void static delete_file(string file_path, int sock);
    void static send_file_name(string file_path, int sock);
    void static receive_list_files(int sock);
};

#endif // FILEINFO_H