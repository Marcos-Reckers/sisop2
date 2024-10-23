#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>

#define PORT 8080
#define BUFFER_SIZE 1024

using namespace std;

typedef struct packet{
    uint16_t type; //Tipo do pacote (p.ex. DATA | CMD)
    uint16_t seqn; //Número de sequência
    uint32_t total_size; //Número total de fragmentos
    uint16_t length; //Comprimento do payload
    const char* _payload; //Dados do pacote
} packet;

void handle_client(int client_socket)
{
    char buffer[BUFFER_SIZE] = {0};
    string client_id, received_text;

    // Lê o ID do cliente
    read(client_socket, buffer, BUFFER_SIZE);
    client_id = string(buffer);
    cout << "Nova conexão recebida. Iniciando thread para " << client_id << endl;

    // Cria/abre o arquivo SERVER_conversa_cliente_<client_id>.txt
    string server_filename = "SERVER_conversa_cliente_" + client_id + ".txt";

    while (true)
    {
        memset(buffer, 0, BUFFER_SIZE); // Limpa o buffer para cada nova mensagem

        // Recebe a mensagem do cliente
        int valread = read(client_socket, buffer, BUFFER_SIZE);
        if (valread <= 0)
        {
            break; // Erro ou cliente desconectado
        }

        received_text = string(buffer);

        // Verifica se o cliente enviou "exit"
        if (received_text == "exit")
        {
            cout << "Cliente " << client_id << " desconectou-se." << endl;
            break;
        }

        // Atualiza o arquivo SERVER_conversa_cliente_<client_id>.txt
        ofstream server_file(server_filename, ios_base::trunc);
        server_file << "Cliente: " << received_text << endl;
        server_file << "Servidor: recebido e copiado" << endl;
        server_file.close();

        // Envia o conteúdo atualizado de volta para o cliente
        ifstream updated_file(server_filename);
        string updated_content((istreambuf_iterator<char>(updated_file)), istreambuf_iterator<char>());
        updated_file.close();

        send(client_socket, updated_content.c_str(), updated_content.size(), 0);
    }

    // Fecha a conexão com o cliente
    close(client_socket);
}



int main()
{
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Cria o socket do servidor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Falha ao criar socket");
        exit(EXIT_FAILURE);
    }

    // Configura o endereço do servidor
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Faz o bind do socket à porta
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Erro no bind");
        exit(EXIT_FAILURE);
    }

    // Coloca o servidor em modo de escuta
    if (listen(server_fd, 3) < 0)
    {
        perror("Erro no listen");
        exit(EXIT_FAILURE);
    }

    cout << "Servidor aguardando conexões na porta " << PORT << "...\n";

    while (true)
    {
        // Aceita uma nova conexão de cliente
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            perror("Falha ao aceitar conexão");
            exit(EXIT_FAILURE);
        }

        thread client_thread(handle_client, client_socket);
        // Cria uma thread para lidar com o cliente

        client_thread.detach(); // Permite que a thread rode independentemente
    }

    return 0;
}
