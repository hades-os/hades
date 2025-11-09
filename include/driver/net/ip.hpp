#ifndef IP_HPP
#define IP_HPP

#include <frg/string.hpp>
#include <cstddef>
#include <cstdint>
#include <driver/net/types.hpp>

namespace net {
    using in_addr_t = uint32_t;
    using in_port_t = uint16_t;

    struct in_addr {
        in_addr_t s_addr;
    };
    
    struct sockaddr_in {
        sa_family_t sin_family;
        in_port_t sin_port;
        in_addr sin_addr;
        uint8_t sin_zero[8];
    };
    
    struct in6_addr {
        union {
            uint8_t __s6_addr[16];
            uint16_t __s6_addr16[8];
            uint32_t __s6_addr32[4];
        } __in6_union;
    };

    struct sockaddr_in6 {
        sa_family_t     sin6_family;
        in_port_t       sin6_port;
        uint32_t        sin6_flowinfo;
        in6_addr sin6_addr;
        uint32_t        sin6_scope_id;
    };
    
    struct ip_mreq {
        struct in_addr imr_multiaddr;
        struct in_addr imr_interface;
    };
    
    struct ip_mreq_source {
        struct in_addr imr_multiaddr;
        struct in_addr imr_interface;
        struct in_addr imr_sourceaddr;
    };
    
    struct ip_mreqn {
        struct in_addr imr_multiaddr;
        struct in_addr imr_address;
        int imr_ifindex;
    };
    
    struct ipv6_mreq {
        struct in6_addr ipv6mr_multiaddr;
        unsigned        ipv6mr_interface;
    };
    
    struct in_pktinfo {
        unsigned int ipi_ifindex;
        struct in_addr ipi_spec_dst;
        struct in_addr ipi_addr;
    };
    
    struct in6_pktinfo {
        struct in6_addr ipi6_addr;
        uint32_t ipi6_ifindex;
    };
    
    struct group_req {
        uint32_t gr_interface;
        struct sockaddr_storage gr_group;
    };
    
    struct group_source_req {
        uint32_t gsr_interface;
        sockaddr_storage gsr_group;
        sockaddr_storage gsr_source;
    };
    
    constexpr int MCAST_INCLUDE = 1;

    constexpr in_addr_t INADDR_ANY = 0x00000000;
    constexpr in_addr_t INADDR_BROADCAST = 0xffffffff;
    constexpr in_addr_t INADDR_NONE = 0xffffffff;
    constexpr in_addr_t INADDR_LOOPBACK = 0x7f000001;
    
    constexpr in_addr_t INADDR_UNSPEC_GROUP  = 0xe0000000;
    constexpr in_addr_t INADDR_ALLHOSTS_GROUP  = 0xe0000001;
    constexpr in_addr_t INADDR_ALLRTRS_GROUP  = 0xe0000002;
    constexpr in_addr_t INADDR_ALLSNOOPERS_GROUP = 0xe000006a;
    constexpr in_addr_t INADDR_MAX_LOCAL_GROUP  = 0xe00000ff;
    
    constexpr in6_addr IN6ADDR_ANY_INIT = { { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } } };
    constexpr in6_addr IN6ADDR_LOOPBACK_INIT = { { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 } } };
    
    constexpr int INET_ADDRSTRLEN = 16;
    constexpr int INET6_ADDRSTRLEN = 46;

    constexpr int IPPORT_RESERVED = 1024;

    constexpr int IPPROTO_IP = 0;
    constexpr int IPPROTO_HOPOPTS = 0;
    constexpr int IPPROTO_ICMP = 1;
    constexpr int IPPROTO_IGMP = 2;
    constexpr int IPPROTO_IPIP = 4;
    constexpr int IPPROTO_TCP = 6;
    constexpr int IPPROTO_EGP = 8;
    constexpr int IPPROTO_PUP = 12;
    constexpr int IPPROTO_UDP = 17;
    constexpr int IPPROTO_IDP = 22;
    constexpr int IPPROTO_TP = 29;
    constexpr int IPPROTO_DCCP = 33;
    constexpr int IPPROTO_IPV6 = 41;
    constexpr int IPPROTO_ROUTING = 43;
    constexpr int IPPROTO_FRAGMENT = 44;
    constexpr int IPPROTO_RSVP = 46;
    constexpr int IPPROTO_GRE = 47;
    constexpr int IPPROTO_ESP = 50;
    constexpr int IPPROTO_AH = 51;
    constexpr int IPPROTO_ICMPV6 = 58;
    constexpr int IPPROTO_NONE = 59;
    constexpr int IPPROTO_DSTOPTS = 60;
    constexpr int IPPROTO_MTP = 92;
    constexpr int IPPROTO_BEETPH = 94;
    constexpr int IPPROTO_ENCAP = 98;
    constexpr int IPPROTO_PIM = 103;
    constexpr int IPPROTO_COMP = 108;
    constexpr int IPPROTO_SCTP = 132;
    constexpr int IPPROTO_MH = 135;
    constexpr int IPPROTO_UDPLITE = 136;
    constexpr int IPPROTO_MPLS = 137;
    constexpr int IPPROTO_RAW = 255;
    constexpr int IPPROTO_MAX = 256;

    constexpr int IP_TOS = 1;
    constexpr int IP_TTL = 2;
    constexpr int IP_HDRINCL = 3;
    constexpr int IP_OPTIONS = 4;
    constexpr int IP_RECVOPTS = 6;
    constexpr int IP_PKTINFO = 8;
    constexpr int IP_PKTOPTIONS = 9;
    constexpr int IP_MTU_DISCOVER = 10;
    constexpr int IP_RECVERR = 11;
    constexpr int IP_RECVTTL = 12;
    constexpr int IP_MTU = 14;
    constexpr int IP_MULTICAST_IF = 32;
    constexpr int IP_MULTICAST_TTL = 33;
    constexpr int IP_MULTICAST_LOOP = 34;
    constexpr int IP_ADD_MEMBERSHIP = 35;
    constexpr int IP_DROP_MEMBERSHIP = 36;
    constexpr int IP_UNBLOCK_SOURCE = 37;
    constexpr int IP_BLOCK_SOURCE = 38;
    constexpr int IP_ADD_SOURCE_MEMBERSHIP = 39;
    constexpr int IP_DROP_SOURCE_MEMBERSHIP = 40;
    constexpr int IP_UNICAST_IF = 50;

    constexpr int IPV6_2292PKTOPTIONS = 6;
    constexpr int IPV6_UNICAST_HOPS = 16;
    constexpr int IPV6_MULTICAST_IF = 17;
    constexpr int IPV6_MULTICAST_HOPS = 18;
    constexpr int IPV6_MULTICAST_LOOP = 19;
    constexpr int IPV6_JOIN_GROUP = 20;
    constexpr int IPV6_LEAVE_GROUP = 21;
    constexpr int IPV6_MTU = 24;
    constexpr int IPV6_RECVERR = 25;
    constexpr int IPV6_V6ONLY = 26;
    constexpr int IPV6_RECVPKTINFO = 49;
    constexpr int IPV6_PKTINFO = 50;
    constexpr int IPV6_RECVHOPLIMIT = 51;
    constexpr int IPV6_HOPLIMIT = 52;
    constexpr int IPV6_TCLASS = 67;
    constexpr int IPV6_ADD_MEMBERSHIP = IPV6_JOIN_GROUP;
    constexpr int IPV6_DROP_MEMBERSHIP = IPV6_LEAVE_GROUP;

    constexpr int IP_PMTUDISC_DONT = 0;
    constexpr int IP_PMTUDISC_WANT = 1;
    constexpr int IP_PMTUDISC_DO = 2;
    constexpr int IP_PMTUDISC_PROBE = 3;
    constexpr int IP_PMTUDISC_INTERFACE = 4;
    constexpr int IP_PMTUDISC_OMIT = 5;

    constexpr int MCAST_BLOCK_SOURCE = 43;
    constexpr int MCAST_UNBLOCK_SOURCE = 44;
    constexpr int MCAST_JOIN_SOURCE_GROUP = 46;
    constexpr int MCAST_LEAVE_SOURCE_GROUP = 47;
    
    constexpr int __UAPI_DEF_IN_ADDR = 0;
    constexpr int __UAPI_DEF_IN_IPPROTO = 0;
    constexpr int __UAPI_DEF_IN_PKTINFO = 0;
    constexpr int __UAPI_DEF_IP_MREQ = 0;
    constexpr int __UAPI_DEF_SOCKADDR_IN = 0;
    constexpr int __UAPI_DEF_IN_CLASS = 0;
    constexpr int __UAPI_DEF_IN6_ADDR = 0;
    constexpr int __UAPI_DEF_IN6_ADDR_ALT = 0;
    constexpr int __UAPI_DEF_SOCKADDR_IN6 = 0;
    constexpr int __UAPI_DEF_IPV6_MREQ = 0;
    constexpr int __UAPI_DEF_IPPROTO_V6 = 0;
    constexpr int __UAPI_DEF_IPV6_OPTIONS = 0;
    constexpr int __UAPI_DEF_IN6_PKTINFO = 0;
    constexpr int __UAPI_DEF_IP6_MTUINFO = 0;

    constexpr size_t ipv4_alen = 4;
    inline uint32_t ipv4_part_parse(const char *s) {
        uint32_t res = 0, idx = 0;

        while (s[idx] >= '0' && s[idx] <= '9') {
            res = 10 * res + (s[idx++] - '0');
        }

        return res;
    }

    inline uint32_t ipv4_pton(const char* ip_str) {
        uint32_t ipv4 = 0;

        frg::string_view view(ip_str);
        size_t dot_idx = 0;
        for (uint32_t i = 0; i < 4; i++) {
            uint32_t part = ipv4_part_parse(view.data());
            if (part > 255) {
                return uint32_t(-1);
            }

            ipv4 += part << (8 * (4 - (i + 1)));

            dot_idx = view.find_first('.');
            view = view.sub_string(dot_idx + 1);
        }

        return ipv4;
    }

    inline char *ipv4_ntop(uint32_t ipv4, char *ipv4_str) {
        uint8_t bytes[4];
        bytes[0] = ipv4 & 0xFF;
        bytes[1] = (ipv4 >> 8) & 0xFF;
        bytes[2] = (ipv4 >> 16) & 0xFF;
        bytes[3] = (ipv4 >> 24) & 0xFF;

        size_t offset = 0;
        uint8_t part_size = 0;
        for (size_t i = 0; i < 4; i++) {
            uint8_t part = bytes[3 - i];
            part_size = 1;
            if (part > 99) {
                part_size = 3;
            } else if (part > 9) {
                part_size = 2;
            }

            util::num_fmt(ipv4_str + offset, part_size + 1, bytes[3 - i], 10, 0, ' ', 0, 0, -1);
            ipv4_str[offset + part_size] = '.';
            offset += part_size + 1;
        }
        
        ipv4_str[offset + part_size] = '\0';
        return ipv4_str;
    }

    static const char *broadcast_ipv4 = "0.0.0.0";

    namespace pkt {
        struct [[gnu::packed]] ipv4 {
            uint8_t ihl : 4;
            uint8_t ver : 4;

            uint8_t diff_serv : 6;
            uint8_t ecn  : 2;

            uint16_t len;
            uint16_t id;

            uint16_t frag_off;

            uint8_t ttl;
            uint8_t proto;
            uint16_t checksum;
            
            uint32_t src_ip;
            uint32_t dest_ip;
        }; 
    }
}

#endif