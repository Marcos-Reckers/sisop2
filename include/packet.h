#ifndef PACKET_H
#define PACKET_H

#include <vector>
#include <cstdint>
#include <string>
#include "fileInfo.h"

#define MAX_PAYLOAD_SIZE 4096

class Packet
{
private:
    uint16_t type;                 // Tipo do pacote
    uint16_t seqn;                 // Número de sequência
    uint32_t total_size;           // Número total de fragmentos
    uint16_t length;               // Comprimento do payload
    std::vector<char> payload;     // Dados do pacote

public:

    Packet();
    Packet(uint16_t type, uint16_t seqn, uint32_t total_size, uint16_t length, const std::vector<char>& payload);
    
    static std::vector<uint8_t> packet_to_bytes(const Packet& pkt);
    static Packet bytes_to_packet(const std::vector<uint8_t>& bytes);

    static Packet create_packet_cmd(const std::string& command);
    static std::vector<Packet> create_packet_data(const std::vector<char>& data);

    static Packet create_packet_info(const FileInfo& file_info);

    static FileInfo packet_to_info(const Packet& pkt);
    
    static ssize_t packet_base_size();

    static std::vector<Packet> create_packets_from_file(const std::string& file_path);

    void print() const;

    uint16_t get_type() const;
    uint16_t get_seqn() const;
    uint32_t get_total_size() const;
    uint16_t get_length() const;
    const std::vector<char>& get_payload() const;

    void set_type(uint16_t t);
    void set_seqn(uint16_t s);
    void set_total_size(uint32_t ts);
    void set_length(uint16_t len);
    void set_payload(const std::vector<char>& pl);
    std::vector<char> info_to_bytes(const FileInfo& file_info);
};

#endif // PACKET_H
