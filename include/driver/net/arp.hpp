#ifndef ARP_HPP
#define ARP_HPP

#include <driver/net/types.hpp>
#include <cstdint>
namespace net {
    namespace pkt {
        constexpr uint16_t arp_req = 1;
        constexpr uint16_t arp_res = 2;

        struct [[gnu::packed]] arp_eth_ipv4 {
            uint16_t host_type;
            uint16_t proto_type;

            uint8_t host_len;
            uint8_t proto_len;

            uint16_t op;
            uint8_t src_addr[eth_alen];
            uint32_t src_ip;

            uint8_t dest_addr[eth_alen];
            uint32_t dest_ip;
        };
    }
}

#endif