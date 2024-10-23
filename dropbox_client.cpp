#include <iostream>
#include <fstream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>

#define BUFFER_SIZE 1024

using namespace std;

typedef struct packet{
    uint16_t type; //Tipo do pacote (p.ex. DATA | CMD)
    uint16_t seqn; //Número de sequência
    uint32_t total_size; //Número total de fragmentos
    uint16_t length; //Comprimento do payload
    const char* _payload; //Dados do pacote
} packet;

void send_file_to_server(int sock, const string &client_id, const string &client_filename) {
    // Lê o conteúdo do arquivo CLIENT_conversa_cliente_<client_id>.txt
    ifstream client_file(client_filename);
    string content((istreambuf_iterator<char>(client_file)), istreambuf_iterator<char>());
    client_file.close();

    // Envia o conteúdo do arquivo para o servidor
    send(sock, content.c_str(), content.size(), 0);
}

void send_packet_to_server(int sock, const string &file_name) {
    ifstream file(file_name, ios::binary | ios::ate);

    // Verifica se o arquivo foi aberto corretamente
    if (!file.is_open()) {
        cout << "Erro ao abrir o arquivo." << endl;
        return;
    }

    // Obtém o tamanho do arquivo
    streamsize file_size = file.tellg();
    file.seekg(0, ios::beg);

    // Envia o nome do arquivo
    send(sock, file_name.c_str(), file_name.size(), 0);

    // Envia o tamanho do arquivo
    send(sock, &file_size, sizeof(file_size), 0);

    // Envia o conteúdo do arquivo em chunks
    char buffer[BUFFER_SIZE];
    while (file.read(buffer, sizeof(buffer))) {
        send(sock, buffer, sizeof(buffer), 0);
    }
    // Envia o resto do arquivo (se o tamanho não for múltiplo de BUFFER_SIZE)
    if (file.gcount() > 0) {
        send(sock, buffer, file.gcount(), 0);
    }

    file.close();
    cout << "Arquivo enviado com sucesso.\n";
}


void receive_file_from_server(int sock, const string &client_filename) {
    char buffer[BUFFER_SIZE] = {0};

    // Recebe o arquivo atualizado do servidor
    int valread = read(sock, buffer, BUFFER_SIZE);
    if (valread > 0) {
        // Atualiza o arquivo CLIENT_conversa_cliente_<client_id>.txt localmente
        ofstream client_file(client_filename, ios_base::trunc); // Substitui o conteúdo anterior
        client_file << buffer;
        client_file.close();

        // Exibe o conteúdo do arquivo no terminal
        cout << "Conteúdo do arquivo atualizado:\n";
        cout << buffer << endl;
    }
}

void send_exit_message(int sock, const string &client_id) {
    // Envia "exit" para o servidor
    send(sock, "exit", 4, 0);
}

int main(int argc, char const *argv[])
{
    std::vector<std::string> args{argv, argv + argc};

    int sock = 0;
    struct sockaddr_in serv_addr;
    string client_id, message;

    client_id = args[1];

    // Cria o arquivo de conversa para o cliente
    string client_filename = "CLIENT_conversa_cliente_" + client_id + ".txt";

    // Cria o socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        cout << "Erro ao criar socket" << endl;
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(args[3].c_str()));

    // Converte o endereço IP para binário
    if (inet_pton(AF_INET, args[2].c_str(), &serv_addr.sin_addr) <= 0) {
        cout << "Endereço inválido ou não suportado" << endl;
        return -1;
    }

    // Conecta ao servidor
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        cout << "Falha na conexão" << endl;
        return -1;
    }

    // Envia o ID do cliente
    send(sock, client_id.c_str(), client_id.size(), 0);

    // Loop de comunicação com o servidor
    while (true) {
        // Solicita o texto a ser adicionado ao arquivo
        cout << "Digite sua mensagem (ou 'exit' para sair): ";
        getline(cin, message);

        // Escreve a mensagem no arquivo CLIENT_conversa_cliente_<client_id>.txt
        ofstream client_file(client_filename, ios_base::app);
        client_file << "Cliente: " << message << endl;
        client_file.close();

        // Envia a mensagem para o servidor
        send_file_to_server(sock, client_id, client_filename);

        // Recebe o arquivo atualizado do servidor
        receive_file_from_server(sock, client_filename);

        // Verifica se a mensagem foi "exit"
        if (message == "exit") {
            send_exit_message(sock, client_id);
            break;
        }
    }

    // Fecha o socket
    close(sock);

    return 0;
}
