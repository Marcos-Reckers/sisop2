#include <iostream>
#include "packet.h"

#include <fstream>
#include <vector>

int main()
{
    Packet pkt = Packet::create_packet_cmd("upload <path/filename.ext>");
    
    std::vector<uint8_t> byteVector = Packet::packet_to_bytes(pkt);
    Packet reconstructedPkt = Packet::bytes_to_packet(byteVector);

    reconstructedPkt.print();

    std::vector<Packet> packets = Packet::create_packets_from_file("test.txt");
    for(const Packet& pkt : packets) {
        pkt.print();
    }

    return 0;
}
