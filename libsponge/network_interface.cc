#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`


using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    EthernetFrame frame;
    frame.header().src = _ethernet_address;
    frame.header().type = EthernetHeader::TYPE_IPv4;
    frame.payload() = dgram.serialize();

    if(_ip_mac_table.find(next_hop_ip) != _ip_mac_table.end()) {
        frame.header().dst = _ip_mac_table[next_hop_ip];
        _frames_out.push(frame);
    }
    else {
        if(_last_send_arp_time.find(next_hop_ip) == _last_send_arp_time.end()
            || _last_send_arp_time[next_hop_ip] > 5000) {
            _last_send_arp_time[next_hop_ip] = 0;
            send_arp_query(next_hop_ip);
        }
        _unsend_frame[next_hop_ip].push_back(frame);
    }
}

void NetworkInterface::send_arp_query(uint32_t ip_num) {
    EthernetFrame query;
    query.header().src = _ethernet_address;
    query.header().type = EthernetHeader::TYPE_ARP;
    query.header().dst = ETHERNET_BROADCAST;
    ARPMessage message;
    message.opcode = ARPMessage::OPCODE_REQUEST;
    message.sender_ip_address = _ip_address.ipv4_numeric();
    message.sender_ethernet_address = _ethernet_address;
    message.target_ip_address = ip_num;
    query.payload() = BufferList{message.serialize()};
    _frames_out.push(query);
}

void NetworkInterface::arp_learn(uint32_t ip_num, EthernetAddress mac) {
    _ip_mac_table[ip_num] = mac;
    _ip_record_time[ip_num] = 0;
    if (_unsend_frame.find(ip_num) != _unsend_frame.end()) {
        for(auto &frame: _unsend_frame[ip_num]) {
            frame.header().dst = mac;
            _frames_out.push(frame);
        }
        _last_send_arp_time.erase(ip_num);
        _unsend_frame.erase(ip_num);
    }
    
}


//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    optional<InternetDatagram> datagram{};
    if(frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST)
        return datagram;
    
    if(frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram ipv4_datagram;
        auto res = ipv4_datagram.parse(frame.payload());
        if (res == ParseResult::NoError) {
            datagram.emplace(ipv4_datagram);
        }   
    } else if(frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp;
        auto parse_res = arp.parse(frame.payload());
        if (parse_res == ParseResult::NoError) {
            arp_learn(arp.sender_ip_address, arp.sender_ethernet_address);
            if (arp.opcode == ARPMessage::OPCODE_REQUEST && arp.target_ip_address == _ip_address.ipv4_numeric()) {
                EthernetFrame reply;
                reply.header().src = _ethernet_address;
                reply.header().type = EthernetHeader::TYPE_ARP;
                reply.header().dst = frame.header().src;
                ARPMessage message;
                message.opcode = ARPMessage::OPCODE_REPLY;
                message.sender_ip_address = _ip_address.ipv4_numeric();
                message.sender_ethernet_address = _ethernet_address;
                message.target_ip_address = arp.sender_ip_address;
                message.target_ethernet_address = arp.sender_ethernet_address;
                reply.payload() = BufferList{message.serialize()};
                _frames_out.push(reply);
            }
            
        }
        
    }
    return datagram;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { 
    for (auto &entry: _ip_record_time) {
        entry.second += ms_since_last_tick;
        if (entry.second > 30000) {
            uint32_t ip = entry.first;
            _ip_record_time.erase(ip);
            _ip_mac_table.erase(ip);
        }
    }
    for (auto &enrty: _last_send_arp_time) {
        enrty.second += ms_since_last_tick;
    }
}
