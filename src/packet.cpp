#include "packet.h"
#include "fileInfo.h"
#include <cstring> // Para memcpy
#include <fstream> // Para ifstream
#include <cmath>   // Para ceil
#include <algorithm>
#include <iostream>
#include <sstream> // Para istringstream

Packet::Packet() : type(0), seqn(0), total_size(0), length(0), payload() {}

Packet::Packet(uint16_t type, uint16_t seqn, uint32_t total_size, uint16_t length, const std::vector<char> &payload)
    : type(type), seqn(seqn), total_size(total_size), length(length), payload(payload) {}

std::vector<uint8_t> Packet::packet_to_bytes(const Packet &pkt)
{
    size_t totalSize = sizeof(pkt.type) + sizeof(pkt.seqn) + sizeof(pkt.total_size) +
                       sizeof(pkt.length) + pkt.length;

    std::vector<uint8_t> bytes(totalSize);
    size_t index = 0;

    std::memcpy(&bytes[index], &pkt.type, sizeof(pkt.type));
    index += sizeof(pkt.type);

    std::memcpy(&bytes[index], &pkt.seqn, sizeof(pkt.seqn));
    index += sizeof(pkt.seqn);

    std::memcpy(&bytes[index], &pkt.total_size, sizeof(pkt.total_size));
    index += sizeof(pkt.total_size);

    std::memcpy(&bytes[index], &pkt.length, sizeof(pkt.length));
    index += sizeof(pkt.length);

    if (!pkt.payload.empty() && pkt.length > 0)
    {
        std::memcpy(&bytes[index], pkt.payload.data(), pkt.length);
    }

    return bytes;
}

Packet Packet::bytes_to_packet(const std::vector<uint8_t> &bytes)
{
    Packet pkt;
    size_t index = 0;

    std::memcpy(&pkt.type, &bytes[index], sizeof(pkt.type));
    index += sizeof(pkt.type);

    std::memcpy(&pkt.seqn, &bytes[index], sizeof(pkt.seqn));
    index += sizeof(pkt.seqn);

    std::memcpy(&pkt.total_size, &bytes[index], sizeof(pkt.total_size));
    index += sizeof(pkt.total_size);

    std::memcpy(&pkt.length, &bytes[index], sizeof(pkt.length));
    index += sizeof(pkt.length);

    if (pkt.length > 0)
    {
        pkt.payload.resize(pkt.length);
        std::memcpy(pkt.payload.data(), &bytes[index], pkt.length);
    }

    return pkt;
}

Packet Packet::create_packet_cmd(const std::string &command)
{
    return Packet(1, 1, 1, command.size() + 1, std::vector<char>(command.begin(), command.end()));
}

std::vector<Packet> Packet::create_packet_data(const std::vector<char> &data)
{
    std::vector<Packet> packets;
    int num_packets = std::ceil(data.size() / (float)MAX_PAYLOAD_SIZE);

    for (int i = 0; i < num_packets; ++i)
    {
        int payload_size = std::min(MAX_PAYLOAD_SIZE, static_cast<int>(data.size() - i * MAX_PAYLOAD_SIZE));
        packets.emplace_back(2, i, num_packets, payload_size,
                             std::vector<char>(data.begin() + i * MAX_PAYLOAD_SIZE,
                                               data.begin() + i * MAX_PAYLOAD_SIZE + payload_size));
    }

    return packets;
}

Packet Packet::create_packet_info(FileInfo &file_info)
{
    Packet pkt;

    pkt.set_type(3);
    pkt.set_seqn(1);
    pkt.set_total_size(1);

    vector<char> payload = info_to_string(file_info);
    // printa o payload para ver se está correto
    for (int i = 0; i < payload.size(); i++)
    {
        std::cout << payload[i];
    }
    std::cout << endl;

    pkt.set_length(payload.size() + 1);

    pkt.set_payload(payload);

    // pkt.print();

    return pkt;
}

std::vector<char> Packet::info_to_string(FileInfo &file_info)
{
    // remover os /n dos timestamps
    // std::ostringstream oss;
    // oss << file_info.get_file_name() << ";" << file_info.get_file_size() << ";" << file_info.get_m_time() << ";" << file_info.get_a_time() << ";" << file_info.get_c_time();
    // std::string str = oss.str();
    // return std::vector<char>(str.begin(), str.end());
    printf("file_info.get_file_name(): %s\n", file_info.get_file_name().c_str());
    return std::vector<char>(file_info.get_file_name().begin(), file_info.get_file_name().end());
}

FileInfo Packet::string_to_info(const std::vector<char> &data)
{
    std::string data_str(data.begin(), data.end());
    std::istringstream iss(data_str);
    std::string token;
    std::vector<std::string> tokens;

    while (std::getline(iss, token, ';'))
    {
        tokens.push_back(token);
    }

    if (tokens.size() != 5)
    {
        std::cerr << "Erro: Dados do pacote incompletos." << std::endl;
        return FileInfo(); // Retorna um objeto FileInfo vazio em caso de erro
    }

    FileInfo file_info;
    file_info.set_file_name(tokens[0]);

    try
    {
        file_info.set_file_size(std::stoi(tokens[1]));
    }
    catch (const std::exception &e)
    {
        std::cerr << "Erro ao converter file_size: " << e.what() << std::endl;
        file_info.set_file_size(0); // Ou outro tratamento de erro
    }

    file_info.set_m_time(tokens[2]);
    file_info.set_a_time(tokens[3]);
    file_info.set_c_time(tokens[4]);

    return file_info;
}

std::vector<Packet> Packet::create_packets_from_file(const std::string &file_path)
{
    std::ifstream infile(file_path, std::ios::binary);
    std::vector<Packet> packets;

    if (!infile.is_open())
    {
        std::cerr << "Não foi possível abrir o arquivo: " << file_path << "\n";
        return packets;
    }

    std::vector<char> file_data((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());

    packets = create_packet_data(file_data);

    infile.close();
    return packets;
}

void Packet::print() const
{
    std::cout << "Type: " << type << "\n";
    std::cout << "SeqNum: " << seqn << "\n";
    std::cout << "Total size: " << total_size << "\n";
    std::cout << "Length: " << length << "\n";
    std::cout << "Payload: " << std::string(payload.begin(), payload.end()) << "\n";
}

ssize_t Packet::packet_base_size() { return sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint16_t); }

uint16_t Packet::get_type() const { return type; }
uint16_t Packet::get_seqn() const { return seqn; }
uint32_t Packet::get_total_size() const { return total_size; }
uint16_t Packet::get_length() const { return length; }
const std::vector<char> &Packet::get_payload() const { return payload; }

void Packet::set_type(uint16_t t) { type = t; }
void Packet::set_seqn(uint16_t s) { seqn = s; }
void Packet::set_total_size(uint32_t ts) { total_size = ts; }
void Packet::set_length(uint16_t len) { length = len; }
void Packet::set_payload(const std::vector<char> &pl) { payload = pl; }
