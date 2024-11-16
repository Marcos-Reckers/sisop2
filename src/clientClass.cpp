#include "clientClass.h"

#include <sys/inotify.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <netdb.h>
#include <filesystem>
#include <thread>
#include <limits.h>

#include "packet.h"
#include "fileInfo.h"

Client::Client(string username, struct hostent *server, string server_port) : username(username), server(server), server_port(server_port) {}

void Client::set_sock(int sock) { this->sock = sock; }


int16_t Client::connect_to_server()
{
    struct sockaddr_in serv_addr;
    // Cria o socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        cout << "Erro ao criar socket" << endl;
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(server_port.c_str()));
    serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
    bzero(&(serv_addr.sin_zero), 8);

    // Tenta conectar ao servidor por 100 segundos
    int attempts = 0;
    while (attempts < 10)
    {
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == 0)
        {
            std::string username_with_null = username + '\0';
            send(sock, username_with_null.c_str(), username_with_null.size(), 0);

            if (end_connection())
            {
                cout << "Não é possível conectar mais do que 2 dispositivos simultâneos." << endl;
                return -2;
            }

            set_sock(sock);
            return sock;
        }
        else
        {
            cout << "Tentativa de conexão falhou, tentando novamente..." << endl;
            sleep(1); // Aguarda 1 segundo antes de tentar novamente
            attempts++;
        }
    }

    cout << "Falha na conexão TIMEOUT" << endl;
    close(sock);
    return -3;
}

void Client::send_cmd(string cmd)
{
    Packet cmd_packet = Packet::create_packet_cmd(cmd);
    std::vector<uint8_t> packet_bytes = Packet::packet_to_bytes(cmd_packet);
    ssize_t sent_bytes = send(sock, packet_bytes.data(), packet_bytes.size(), 0);

    if (sent_bytes < 0)
    {
        std::cerr << "Erro ao enviar comando como pacote." << std::endl;
        return;
    }

    std::cout << "Comando enviado como pacote: " << cmd << std::endl;

}

void Client::get_sync_dir()
{
    send_cmd("get_sync_dir");
}

bool Client::end_connection()
{
    char buffer[256];
    recv(sock, buffer, 256, 0);
    if (strcmp(buffer, "exit") == 0)
    {
        close(sock);
        return true;
    }
    return false;
}

// void Client::monitor_sync_dir() {
//     create_dir("sync_dir");
//     std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
//     std::string sync_dir = exec_path + "/sync_dir";

//     int fd = inotify_init();
//     if (fd < 0) {
//         perror("inotify_init");
//         return;
//     }

//     int wd = inotify_add_watch(fd, sync_dir.c_str(), IN_CREATE | IN_DELETE | IN_MODIFY);
//     if (wd < 0) {
//         perror("inotify_add_watch");
//         close(fd);
//         return;
//     }

//     const size_t buf_size = 1024 * (sizeof(struct inotify_event) + NAME_MAX + 1);
//     char *buffer = new char[buf_size];

//     while (true) {
//         int length = read(fd, buffer, buf_size);
//         if (length < 0) {
//             perror("read");
//             break;
//         }

//         int i = 0;
//         while (i < length) {
//             struct inotify_event *event = (struct inotify_event *) &buffer[i];

//             if (event->len) {
//                 if (event->mask & IN_CREATE) {
//                     // std::cout << "Arquivo criado: " << event->name << std::endl;
//                     send_cmd("upload");
//                     send_file(sync_dir + "/" + std::string(event->name), sock);

//             }
//                 // if (event->mask & IN_DELETE) {
//                 //     // std::cout << "Arquivo excluído: " << event->name << std::endl;
//                 //     send_cmd("delete");
//                 //     send_file_name(event->name);
//                 // }
//                 // if (event->mask & IN_MODIFY) {
//                 //     // std::cout << "Arquivo modificado: " << event->name << std::endl;
//                 //     send_cmd("upload");
//                 //     send_file(sync_dir + "/" + std::string(event->name));
//                 // }
//             }

//             i += sizeof(struct inotify_event) + event->len;
//         }
//     }

//     delete[] buffer;
//     inotify_rm_watch(fd, wd);
//     close(fd);
// }