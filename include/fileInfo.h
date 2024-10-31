#ifndef FILEINFO_H
#define FILEINFO_H

#include <string>

using namespace std;

class FileInfo {
private:
    string file_name;
    int file_size;
    string m_time;
    string a_time;
    string c_time;

public:
    FileInfo();

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

};       

#endif // FILEINFO_H