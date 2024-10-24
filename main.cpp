#include <iostream>
#include "packet.h"

int main()
{
    // Create a command packet
    Packet pkt = Packet::create_packet_cmd("upload <path/filename.ext>");
    
    // Convert to bytes and back
    std::vector<uint8_t> byteVector = Packet::packet_to_bytes(pkt);
    Packet reconstructedPkt = Packet::bytes_to_packet(byteVector);

    // Output the results
    std::cout << "Type: " << reconstructedPkt.get_type() << "\n";
    std::cout << "SeqNum: " << reconstructedPkt.get_seqn() << "\n";
    std::cout << "Total size: " << reconstructedPkt.get_total_size() << "\n";
    std::cout << "Length: " << reconstructedPkt.get_length() << "\n";
    std::cout << "Payload: " << std::string(reconstructedPkt.get_payload().begin(), reconstructedPkt.get_payload().end()) << "\n";

    return 0;
}
