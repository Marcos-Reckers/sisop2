#include "packet.h"
#include <cstring>  // Para memcpy
#include <cmath>    // Para ceil
#include <algorithm>

Packet::Packet() : type(0), seqn(0), total_size(0), length(0), payload() {}

Packet::Packet(uint16_t type, uint16_t seqn, uint32_t total_size, uint16_t length, const std::vector<char>& payload)
    : type(type), seqn(seqn), total_size(total_size), length(length), payload(payload) {}

std::vector<uint8_t> Packet::packet_to_bytes(const Packet& pkt)
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

Packet Packet::bytes_to_packet(const std::vector<uint8_t>& bytes)
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

Packet Packet::create_packet_cmd(const std::string& command)
{
    return Packet(1, 1, 1, command.size() + 1, std::vector<char>(command.begin(), command.end()));
}

std::vector<Packet> Packet::create_packet_data(const std::vector<char>& data)
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

uint16_t Packet::get_type() const { return type; }
uint16_t Packet::get_seqn() const { return seqn; }
uint32_t Packet::get_total_size() const { return total_size; }
uint16_t Packet::get_length() const { return length; }
const std::vector<char>& Packet::get_payload() const { return payload; }

void Packet::set_type(uint16_t t) { type = t; }
void Packet::set_seqn(uint16_t s) { seqn = s; }
void Packet::set_total_size(uint32_t ts) { total_size = ts; }
void Packet::set_length(uint16_t len) { length = len; }
void Packet::set_payload(const std::vector<char>& pl) { payload = pl; }
