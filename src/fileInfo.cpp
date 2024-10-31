#include "fileInfo.h"
#include <string>
#include <sys/stat.h>
#include <ctime>
#include <filesystem>
#include <iostream>

using namespace std;

FileInfo::FileInfo() : file_name(""), file_size(0), m_time(""), a_time(""), c_time("") {}

void FileInfo::retrieve_info_from_file(string path)
{
    struct stat file_stat;

    if (stat(path.c_str(), &file_stat) != 0) {
        cout << "Não conseguiu pegar informações do arquivo" << endl;
    }

    file_name = filesystem::path(path).filename().string();
    file_size = file_stat.st_size;
    m_time = ctime(&file_stat.st_mtime);
    a_time = ctime(&file_stat.st_atime);
    c_time = ctime(&file_stat.st_ctime);
}

void FileInfo::print()
{
    cout << "Nome do arquivo: " << file_name << endl;
    cout << "Tamanho do arquivo: " << file_size << " bytes" << endl;
    cout << "Data de modificação: " << m_time;
    cout << "Data de acesso: " << a_time;
    cout << "Data de criação: " << c_time;
}



