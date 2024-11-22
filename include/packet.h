#ifndef PACKET_H
#define PACKET_H

#include <vector>
#include <cstdint>
#include <string>

#define MAX_PAYLOAD_SIZE 4096

class FileInfo; // Forward declaration

class Packet
{
private:
    uint16_t type;             // Tipo do pacote
    uint16_t seqn;             // Número de sequência
    uint32_t total_pakets;       // Número total de fragmentos
    uint16_t payload_size;           // Comprimento do payload
    std::vector<char> payload; // Dados do pacote

public:
    Packet();
    Packet(uint16_t type, uint16_t seqn, uint32_t total_pakets, uint16_t payload_size, const std::vector<char> &payload);

    static std::vector<uint8_t> packet_to_bytes(const Packet &pkt);
    static Packet bytes_to_packet(const std::vector<uint8_t> &bytes);

    static Packet create_packet_cmd(const std::string &command);
    static std::vector<Packet> create_packet_data(const std::vector<char> &data, int type);

    static Packet create_packet_info(FileInfo &file_info, int type);
    static std::vector<char> info_to_string(FileInfo &file_info);
    static FileInfo string_to_info(const std::vector<char> &data);

    static ssize_t packet_header_size();

    static std::vector<Packet> create_packets_from_file(const std::string &file_path, int type);

    void print() const;

    uint16_t get_type() const;
    uint16_t get_seqn() const;
    uint32_t get_total_packets() const;
    uint16_t get_payload_size() const;
    const std::vector<char> &get_payload() const;
    std::string get_payload_as_string() const;

    void set_type(uint16_t t);
    void set_seqn(uint16_t n);
    void set_total_packets(uint32_t n_total);
    void set_payload_size(uint16_t payload_s);
    void set_payload(const std::vector<char> &payload_content);
    void clean_payload();
};

#endif // PACKET_H
