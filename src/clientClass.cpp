#include "clientClass.h"


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

void Client::get_sync_dir()
{
    FileInfo::send_cmd("get_sync_dir", sock);
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

void Client::handle_sync(int sock)
{
    FileInfo::monitor_sync_dir("sync_dir", sock);
}

void Client::handle_upload_request()
{
    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    std::string directory = exec_path + "sync_dir" + "/";
    
    FileInfo::receive_file(directory, sock);
}

void Client::handle_download_request()
{
    char file_name_buffer[256] = {0};
    ssize_t received_bytes = recv(sock, file_name_buffer, sizeof(file_name_buffer), 0);

    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    std::string file_path = exec_path + "sync_dir" + "/" + file_name_buffer;
    std::cout << "Arquivo a ser enviado: " << file_path << std::endl;

    if (received_bytes <= 0)
    {
        std::cerr << "Erro ao receber o nome do arquivo." << std::endl;
        return;
    }

    if (!std::filesystem::exists(file_path))
    {
        std::cerr << "Arquivo não encontrado." << std::endl;
        std::string error_msg = "ERROR: Arquivo não encontrado.";
        Packet error_packet = Packet::create_packet_cmd(error_msg);
        std::vector<uint8_t> error_packet_bytes = Packet::packet_to_bytes(error_packet);
        send(sock, error_packet_bytes.data(), error_packet_bytes.size(), 0);
        return;
    }

    FileInfo::send_file(file_path, sock);
}

void Client::handle_delete_request()
{
    char file_name_buffer[256] = {0};
    ssize_t received_bytes = recv(sock, file_name_buffer, sizeof(file_name_buffer), 0);
    if(received_bytes <= 0)
    {
        std::cerr << "Erro ao receber o nome do arquivo." << std::endl;
        return;
    }

    std::string exec_path = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    std::string path = exec_path + "sync_dir" + "/" + file_name_buffer;

    FileInfo::delete_file(path, sock);
}





