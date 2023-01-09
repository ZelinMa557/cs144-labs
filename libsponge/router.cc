#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    Entry entry{route_prefix, prefix_length, next_hop, interface_num};
    _table.push_back(std::move(entry));
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    uint32_t dst = dgram.header().dst;
    int best_idx = -1;
    uint8_t best_prelen = 0;
    for(size_t i = 0; i < _table.size(); i++) {
        if(match(_table[i], dst) && _table[i].prefix_length >= best_prelen) {
            best_idx = static_cast<int>(i);
            best_prelen = _table[i].route_prefix;
        }
    }
    if(best_idx != -1) {
        if (dgram.header().ttl > 1) {
            dgram.header().ttl--;
            if(_table[best_idx].next_hop.has_value()) {
                interface(_table[best_idx].interface_num).send_datagram(dgram, _table[best_idx].next_hop.value());
                std::cerr << "route to: " <<_table[best_idx].next_hop.value().to_string()<<" on interface "<<_table[best_idx].interface_num<<std::endl;
            } else {
                interface(_table[best_idx].interface_num).send_datagram(dgram, Address::from_ipv4_numeric(dgram.header().dst));
                std::cerr << "route to: " <<Address::from_ipv4_numeric(dgram.header().dst).to_string()<<" on interface "<<_table[best_idx].interface_num<<std::endl;
            }
        }
        
    }
    else {
        std::cerr << "cannot rout ip: " << Address::from_ipv4_numeric(dgram.header().dst).to_string()<<std::endl;
    }
}

bool Router::match(const Entry entry, const uint32_t ip) {
    if(entry.prefix_length == 0)
        return true;
    uint32_t musk = 0xffffffff << (32 - static_cast<uint32_t>(entry.prefix_length));
    return (ip & musk) == (entry.route_prefix & musk);
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
