#include "fileInfo.h"
#include <string>
#include <sys/stat.h>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <algorithm>

using namespace std;

FileInfo::FileInfo() : file_name(""), file_size(0), m_time(""), a_time(""), c_time("") {}
FileInfo::FileInfo(string file_name, int file_size, string m_time, string a_time, string c_time) : file_name(file_name), file_size(file_size), m_time(m_time), a_time(a_time), c_time(c_time) {}

void FileInfo::retrieve_info_from_file(string path)
{
    struct stat file_stat;

    if (stat(path.c_str(), &file_stat) != 0) {
        cout << "Não conseguiu pegar informações do arquivo" << endl;
    }

    file_name = filesystem::path(path).filename().string();
    file_size = file_stat.st_size;

    m_time = ctime(&file_stat.st_mtime);
    m_time.erase(remove(m_time.begin(), m_time.end(), '\n'), m_time.end());

    a_time = ctime(&file_stat.st_atime);
    a_time.erase(remove(a_time.begin(), a_time.end(), '\n'), a_time.end());

    c_time = ctime(&file_stat.st_ctime);
    c_time.erase(remove(c_time.begin(), c_time.end(), '\n'), c_time.end());
}

void FileInfo::print()
{
    cout << "Nome do arquivo: " << file_name << endl;
    cout << "Tamanho do arquivo: " << file_size << " bytes" << endl;
    cout << "Data de modificação: " << m_time << endl;
    cout << "Data de acesso: " << a_time << endl;
    cout << "Data de criação: " << c_time << endl;
}



