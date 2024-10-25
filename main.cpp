#include <iostream>
#include "packet.h"

int main()
{
    Packet pkt = Packet::create_packet_cmd("upload <path/filename.ext>");
    
    std::vector<uint8_t> byteVector = Packet::packet_to_bytes(pkt);
    Packet reconstructedPkt = Packet::bytes_to_packet(byteVector);

    reconstructedPkt.print();

    return 0;
}
