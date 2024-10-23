#include <iostream>
#include <vector>
#include <cstring> // Para memcpy
#include <cstdint> // Para tipos como uint16_t e uint32_t
#include <string>
#include <cmath>
#include <algorithm>

#define MAX_PAYLOAD_SIZE 4096

typedef struct packet
{
    uint16_t type;              // Tipo do pacote
    uint16_t seqn;            // Número de sequência
    uint32_t total_size;        // Número total de fragmentos
    uint16_t length;            // Comprimento do payload
    std::vector<char> _payload; // Dados do pacote (ponteiro para o payload)
} packet;

// Função para converter a struct `packet` em um vetor de bytes
std::vector<uint8_t> packet_to_bytes(const packet &pkt)
{
    // Calcula o tamanho necessário para o vetor de bytes
    size_t total_size = sizeof(pkt.type) + sizeof(pkt.seqn) + sizeof(pkt.total_size) +
                        sizeof(pkt.length) + pkt.length; // O tamanho do payload é `pkt.length`

    // Cria um vetor de bytes com o tamanho necessário
    std::vector<uint8_t> bytes(total_size);

    // Usa um índice para copiar os dados na ordem correta
    size_t index = 0;

    // Copia os campos primitivos para o vetor de bytes
    std::memcpy(&bytes[index], &pkt.type, sizeof(pkt.type));
    index += sizeof(pkt.type);

    std::memcpy(&bytes[index], &pkt.seqn, sizeof(pkt.seqn));
    index += sizeof(pkt.seqn);

    std::memcpy(&bytes[index], &pkt.total_size, sizeof(pkt.total_size));
    index += sizeof(pkt.total_size);

    std::memcpy(&bytes[index], &pkt.length, sizeof(pkt.length));
    index += sizeof(pkt.length);

    // Copia o payload para o vetor de bytes, se o payload for não-nulo e o comprimento for > 0
    if (!pkt._payload.empty() && pkt.length > 0)
    {
        std::memcpy(&bytes[index], pkt._payload.data(), pkt.length);
    }

    return bytes;
}

// Função para converter um vetor de bytes de volta para a struct `packet`
packet bytes_to_packet(const std::vector<uint8_t> &bytes)
{
    packet pkt;
    size_t index = 0;

    // Reconstrói os campos primitivos da struct `packet`
    std::memcpy(&pkt.type, &bytes[index], sizeof(pkt.type));
    index += sizeof(pkt.type);

    std::memcpy(&pkt.seqn, &bytes[index], sizeof(pkt.seqn));
    index += sizeof(pkt.seqn);

    std::memcpy(&pkt.total_size, &bytes[index], sizeof(pkt.total_size));
    index += sizeof(pkt.total_size);

    std::memcpy(&pkt.length, &bytes[index], sizeof(pkt.length));
    index += sizeof(pkt.length);

    // Aloca memória para o payload e copia os dados do vetor de bytes, se o comprimento for > 0
    if (pkt.length > 0)
    {
        char *payload = new char[pkt.length];
        std::memcpy(payload, &bytes[index], pkt.length);
        pkt._payload = std::vector<char>(payload, payload + pkt.length);
    }
    else
    {
        pkt._payload = std::vector<char>();
    }

    return pkt;
}

packet create_package_CMD(std::string command)
{
    packet pkt;
    pkt.type = 1;
    pkt.seqn = 1;
    pkt.total_size = 1;
    pkt.length = command.size() + 1;
    pkt._payload = std::vector<char>(command.begin(), command.end());

    return pkt;
}

std::vector<packet> create_package_DATA(std::vector<char> &data)
{
    std::vector<packet> packets;
    int num_packets = std::ceil(data.size() / (float)MAX_PAYLOAD_SIZE);

    for (int i = 0; i < num_packets; i++)
    {
        packet pkt;
        pkt.type = 2;
        pkt.seqn = i;
        pkt.total_size = num_packets;
        pkt.length = std::min(MAX_PAYLOAD_SIZE, static_cast<int>(data.size() - i * MAX_PAYLOAD_SIZE));
        pkt._payload = std::vector<char>(data.begin() + i * MAX_PAYLOAD_SIZE, data.begin() + i * MAX_PAYLOAD_SIZE + pkt.length);
        packets.push_back(pkt);
    }

    return packets;
}

int main()
{
    // Cria um pacote com um comando
    packet pkt = create_package_CMD("upload <path/filename.ext>");
    // Converte o pacote para um vetor de bytes
    std::vector<uint8_t> byteVector = packet_to_bytes(pkt);

    // Converte o vetor de bytes de volta para um pacote
    packet reconstructedPkt = bytes_to_packet(byteVector);

    // Mostra os dados para verificar se a reconstrução foi bem-sucedida
    std::cout << "Type: " << reconstructedPkt.type << "\n";
    std::cout << "SeqNum: " << reconstructedPkt.seqn << "\n";
    std::cout << "Total size: " << reconstructedPkt.total_size << "\n";
    std::cout << "Length: " << reconstructedPkt.length << "\n";
    std::cout << "Payload: " << std::string(reconstructedPkt._payload.begin(), reconstructedPkt._payload.end()) << "\n";

    // Libera a memória alocada para o payload no pacote reconstruído
    // No need to delete since we are using std::vector

    return 0;
}
