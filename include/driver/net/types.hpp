#ifndef NET_TYPES_HPP
#define NET_TYPES_HPP

#include <util/log/log.hpp>
#include <frg/string.hpp>
#include <mm/mm.hpp>
#include <cstddef>
#include <cstdint>

namespace net {
    constexpr uint32_t ntohl(uint32_t x) { return __builtin_bswap32(x); }
    constexpr uint16_t ntohs(uint16_t x) { return __builtin_bswap16(x); }

    constexpr uint32_t htonl(uint32_t x) { return __builtin_bswap32(x); }
    constexpr uint16_t htons(uint16_t x) { return __builtin_bswap16(x); }

    constexpr size_t eth_alen = 6;
    struct [[gnu::packed]] eth {
        uint8_t dest[eth_alen];
        uint8_t src[eth_alen];
        uint16_t type;
    };

    using mac = uint8_t[6];
    constexpr mac broadcast_mac = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    constexpr bool is_same_mac(mac a, mac b) {
        for (size_t i = 0; i < 6; i++) {
            if (a[i] != b[i]) {
                return false;
            }
        }

        return true;
    }
}

#endif