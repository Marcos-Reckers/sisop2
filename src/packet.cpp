#include "packet.h"
#include <iostream>
#include <cmath>
#include <cstring>
#include "fileInfo.h"

Packet::Packet() : type(0), seqn(0), total_pakets(0), payload_size(0), payload() {}

Packet::Packet(uint16_t type, uint16_t seqn, uint32_t total_pakets, uint16_t payload_size, const std::vector<char> &payload)
    : type(type), seqn(seqn), total_pakets(total_pakets), payload_size(payload_size), payload(payload) {}

std::vector<uint8_t> Packet::packet_to_bytes(const Packet &pkt)
{
    size_t totalSize = sizeof(pkt.type) + sizeof(pkt.seqn) + sizeof(pkt.total_pakets) +
                       sizeof(pkt.payload_size) + pkt.payload_size;

    std::vector<uint8_t> bytes(totalSize);
    size_t index = 0;

    std::memcpy(&bytes[index], &pkt.type, sizeof(pkt.type));
    index += sizeof(pkt.type);

    std::memcpy(&bytes[index], &pkt.seqn, sizeof(pkt.seqn));
    index += sizeof(pkt.seqn);

    std::memcpy(&bytes[index], &pkt.total_pakets, sizeof(pkt.total_pakets));
    index += sizeof(pkt.total_pakets);

    std::memcpy(&bytes[index], &pkt.payload_size, sizeof(pkt.payload_size));
    index += sizeof(pkt.payload_size);

    if (!pkt.payload.empty() && pkt.payload_size > 0)
    {
        std::memcpy(&bytes[index], pkt.payload.data(), pkt.payload_size);
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

    std::memcpy(&pkt.total_pakets, &bytes[index], sizeof(pkt.total_pakets));
    index += sizeof(pkt.total_pakets);

    std::memcpy(&pkt.payload_size, &bytes[index], sizeof(pkt.payload_size));
    index += sizeof(pkt.payload_size);

    if (pkt.payload_size > 0)
    {
        pkt.payload.resize(pkt.payload_size);
        std::memcpy(pkt.payload.data(), &bytes[index], pkt.payload_size);
    }

    return pkt;
}

Packet Packet::create_packet_cmd(const std::string &command)
{
    std::vector<char> payload = std::vector<char>(command.begin(), command.end());
    // verifica o tamnaho do payload + base_size, diminui de 4106 e adiciona "|"s para completar
    int complete_payload = packet_header_size() + MAX_PAYLOAD_SIZE - (packet_header_size() + payload.size());
    for (int i = 0; i < complete_payload; i++)
    {
        payload.push_back('|');
    }
    if (command.find("exit_response") != std::string::npos)
    {
        return Packet(3, 1, 1, payload.size(), payload);
    }
    else if (command.find("sync") != std::string::npos)
    {
        return Packet(2, 1, 1, payload.size(), payload);
    }
    else if (command.find("broadcast") != std::string::npos)
    {
        return Packet(4, 1, 1, payload.size(), payload);
    }
    else if ((command.find("list_server_response") != std::string::npos) || (command.find("download_response") != std::string::npos))
    {
        return Packet(5, 1, 1, payload.size(), payload);
    }
    else
    {
        return Packet(1, 1, 1, payload.size(), payload);
    }
}

std::vector<Packet> Packet::create_packet_data(const std::vector<char> &data, int type)
{
    std::vector<Packet> packets;
    int num_packets = std::ceil(data.size() / (float)MAX_PAYLOAD_SIZE);

    for (int i = 0; i < num_packets; ++i)
    {
        int payload_size = std::min(MAX_PAYLOAD_SIZE, static_cast<int>(data.size() - i * MAX_PAYLOAD_SIZE));
        packets.emplace_back(type, i + 3, num_packets + 2, payload_size,
                             std::vector<char>(data.begin() + i * MAX_PAYLOAD_SIZE,
                                               data.begin() + i * MAX_PAYLOAD_SIZE + payload_size));
    }

    return packets;
}

Packet Packet::create_packet_info(FileInfo &file_info, int type)
{
    Packet pkt;

    pkt.set_type(type);
    pkt.set_seqn(2);
    pkt.set_total_packets(2);

    std::vector<char> payload = info_to_string(file_info);
    // verifica o tamnaho do payload + base_size, diminui de 4106 e adiciona "|"s para completar
    int complete_payload = 4106 - (packet_header_size() + payload.size());
    for (int i = 0; i < complete_payload; i++)
    {
        payload.push_back('|');
    }
    pkt.set_payload_size(payload.size());
    pkt.set_payload(payload);

    return pkt;
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
        return FileInfo();
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

std::vector<Packet> Packet::create_packets_from_file(const std::string &file_path, int type)
{
    std::ifstream infile(file_path, std::ios::binary);
    std::vector<Packet> packets;

    if (!infile.is_open())
    {
        std::cerr << "Não foi possível abrir o arquivo: " << file_path << "\n";
        return packets;
    }

    std::vector<char> file_data((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());

    packets = create_packet_data(file_data, type);

    infile.close();
    return packets;
}

void Packet::print() const
{
    std::cout << "Type: " << type << "\n";
    std::cout << "SeqNum: " << seqn << "\n";
    std::cout << "Total size: " << total_pakets << "\n";
    std::cout << "Length: " << payload_size << "\n";
    std::cout << "Payload: " << std::string(payload.begin(), payload.end()) << "\n";
}

ssize_t Packet::packet_header_size() { return sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint16_t); }

uint16_t Packet::get_type() const { return type; }
uint16_t Packet::get_seqn() const { return seqn; }
uint32_t Packet::get_total_packets() const { return total_pakets; }
uint16_t Packet::get_payload_size() const { return payload_size; }
const std::vector<char> &Packet::get_payload() const { return payload; }

void Packet::set_type(uint16_t t) { type = t; }
void Packet::set_seqn(uint16_t n) { seqn = n; }
void Packet::set_total_packets(uint32_t n_total) { total_pakets = n_total; }
void Packet::set_payload_size(uint16_t payload_s) { payload_size = payload_s; }
void Packet::set_payload(const std::vector<char> &payload_content) { payload = payload_content; }

std::vector<char> Packet::info_to_string(FileInfo &file_info)
{
    std::ostringstream oss;
    oss << file_info.get_file_name() << ";"
        << file_info.get_file_size() << ";"
        << file_info.get_m_time() << ";"
        << file_info.get_a_time() << ";"
        << file_info.get_c_time();
    std::string str = oss.str();
    return std::vector<char>(str.begin(), str.end());
}

std::string Packet::get_payload_as_string() const
{
    return std::string(payload.begin(), payload.end());
}

void Packet::clean_payload()
{
    string dirty_payload = get_payload_as_string();
    string clean_payload = dirty_payload.substr(0, dirty_payload.find('|'));
    set_payload(vector<char>(clean_payload.begin(), clean_payload.end()));
}
