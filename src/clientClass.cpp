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

void Client::handle_sync_request(int sock)
{
    while (true)
    {
        ssize_t total_size = Packet::packet_base_size() + MAX_PAYLOAD_SIZE;
        std::vector<uint8_t> packet_buffer(total_size);
        ssize_t received_bytes = recv(sock, packet_buffer.data(), packet_buffer.size(), 0);

        if (received_bytes < 0)
        {
            std::cerr << "Erro ao receber o pacote." << std::endl;
            return;
        }

        Packet pkt = Packet::bytes_to_packet(packet_buffer);
        pkt.print();

        if (pkt.get_type() == 1)
        {
            std::vector<char> cmd_vec = pkt.get_payload();
            std::string cmd(cmd_vec.begin(), cmd_vec.end());
            std::cout << "Comando recebido: " << cmd << std::endl;
            
            if (cmd.substr(0, 6) == "upload")
            {
                handle_upload_request();
            }
            if (cmd.substr(0, 6) == "delete")
            {
                handle_delete_request();
            }
        }
        else
        {
            std::cerr << "Pacote recebido não é do tipo 1." << std::endl;
        }
    }
}

void Client::handle_sync(int sock)
{
    FileInfo::send_cmd("get_sync_dir", sock);
    FileInfo::monitor_sync_dir("sync_dir", sock);
}

void Client::handle_upload_request()
{    
    FileInfo::receive_file("/sync_dir/", sock);
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
    std::string path = "/sync_dir/" + std::string(file_name_buffer);

    FileInfo::delete_file(path, sock);
}