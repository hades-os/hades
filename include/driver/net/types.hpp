#ifndef NET_TYPES_HPP
#define NET_TYPES_HPP

#include <util/log/log.hpp>
#include <mm/mm.hpp>
#include <cstddef>
#include <cstdint>

namespace net {
    constexpr int SCM_RIGHTS = 1;
    constexpr int SCM_CREDENTIALS = 2;

    constexpr int SHUT_RD = 0;
    constexpr int SHUT_WR = 1;
    constexpr int SHUT_RDWR = 2;

    constexpr int SOCK_STREAM = 1;
    constexpr int SOCK_DGRAM = 2;
    constexpr int SOCK_RAW = 3;
    constexpr int SOCK_RDM = 4;
    constexpr int SOCK_SEQPACKET = 5;
    constexpr int SOCK_DCCP = 6;
    constexpr int SOCK_PACKET = 10;
    constexpr int SOCK_CLOEXEC = 02000000;
    constexpr int SOCK_NONBLOCK = 04000;

    constexpr int PF_UNSPEC = 0;
    constexpr int PF_LOCAL = 1;
    constexpr int PF_UNIX = PF_LOCAL;
    constexpr int PF_FILE = PF_LOCAL;
    constexpr int PF_INET = 2;
    constexpr int PF_AX25 = 3;
    constexpr int PF_IPX = 4;
    constexpr int PF_APPLETALK = 5;
    constexpr int PF_NETROM = 6;
    constexpr int PF_BRIDGE = 7;
    constexpr int PF_ATMPVC = 8;
    constexpr int PF_X25 = 9;
    constexpr int PF_INET6 = 10;
    constexpr int PF_ROSE = 11;
    constexpr int PF_DECnet = 12;
    constexpr int PF_NETBEUI = 13;
    constexpr int PF_SECURITY = 14;
    constexpr int PF_KEY = 15;
    constexpr int PF_NETLINK = 16;
    constexpr int PF_ROUTE = PF_NETLINK;
    constexpr int PF_PACKET = 17;
    constexpr int PF_ASH = 18;
    constexpr int PF_ECONET = 19;
    constexpr int PF_ATMSVC = 20;
    constexpr int PF_RDS = 21;
    constexpr int PF_SNA = 22;
    constexpr int PF_IRDA = 23;
    constexpr int PF_PPPOX = 24;
    constexpr int PF_WANPIPE = 25;
    constexpr int PF_LLC = 26;
    constexpr int PF_IB = 27;
    constexpr int PF_MPLS = 28;
    constexpr int PF_CAN = 29;
    constexpr int PF_TIPC = 30;
    constexpr int PF_BLUETOOTH = 31;
    constexpr int PF_IUCV = 32;
    constexpr int PF_RXRPC = 33;
    constexpr int PF_ISDN = 34;
    constexpr int PF_PHONET = 35;
    constexpr int PF_IEEE802154 = 36;
    constexpr int PF_CAIF = 37;
    constexpr int PF_ALG = 38;
    constexpr int PF_NFC = 39;
    constexpr int PF_VSOCK = 40;
    constexpr int PF_KCM = 41;
    constexpr int PF_QIPCRTR = 42;
    constexpr int PF_SMC = 43;
    constexpr int PF_XDP = 44;
    constexpr int PF_MAX = 45;

    constexpr int AF_UNSPEC = PF_UNSPEC;
    constexpr int AF_LOCAL = PF_LOCAL;
    constexpr int AF_UNIX = AF_LOCAL;
    constexpr int AF_FILE = AF_LOCAL;
    constexpr int AF_INET = PF_INET;
    constexpr int AF_AX25 = PF_AX25;
    constexpr int AF_IPX = PF_IPX;
    constexpr int AF_APPLETALK = PF_APPLETALK;
    constexpr int AF_NETROM = PF_NETROM;
    constexpr int AF_BRIDGE = PF_BRIDGE;
    constexpr int AF_ATMPVC = PF_ATMPVC;
    constexpr int AF_X25 = PF_X25;
    constexpr int AF_INET6 = PF_INET6;
    constexpr int AF_ROSE = PF_ROSE;
    constexpr int AF_DECnet = PF_DECnet;
    constexpr int AF_NETBEUI = PF_NETBEUI;
    constexpr int AF_SECURITY = PF_SECURITY;
    constexpr int AF_KEY = PF_KEY;
    constexpr int AF_NETLINK = PF_NETLINK;
    constexpr int AF_ROUTE = PF_ROUTE;
    constexpr int AF_PACKET = PF_PACKET;
    constexpr int AF_ASH = PF_ASH;
    constexpr int AF_ECONET = PF_ECONET;
    constexpr int AF_ATMSVC = PF_ATMSVC;
    constexpr int AF_RDS = PF_RDS;
    constexpr int AF_SNA = PF_SNA;
    constexpr int AF_IRDA = PF_IRDA;
    constexpr int AF_PPPOX = PF_PPPOX;
    constexpr int AF_WANPIPE = PF_WANPIPE;
    constexpr int AF_LLC = PF_LLC;
    constexpr int AF_IB = PF_IB;
    constexpr int AF_MPLS = PF_MPLS;
    constexpr int AF_CAN = PF_CAN;
    constexpr int AF_TIPC = PF_TIPC;
    constexpr int AF_BLUETOOTH = PF_BLUETOOTH;
    constexpr int AF_IUCV = PF_IUCV;
    constexpr int AF_RXRPC = PF_RXRPC;
    constexpr int AF_ISDN = PF_ISDN;
    constexpr int AF_PHONET = PF_PHONET;
    constexpr int AF_IEEE802154 = PF_IEEE802154;
    constexpr int AF_CAIF = PF_CAIF;
    constexpr int AF_ALG = PF_ALG;
    constexpr int AF_NFC = PF_NFC;
    constexpr int AF_VSOCK = PF_VSOCK;
    constexpr int AF_KCM = PF_KCM;
    constexpr int AF_QIPCRTR = PF_QIPCRTR;
    constexpr int AF_SMC = PF_SMC;
    constexpr int AF_XDP = PF_XDP;
    constexpr int AF_MAX = PF_MAX;

    constexpr int SO_DEBUG = 1;
    constexpr int SO_REUSEADDR = 2;
    constexpr int SO_TYPE = 3;
    constexpr int SO_ERROR = 4;
    constexpr int SO_DONTROUTE = 5;
    constexpr int SO_BROADCAST = 6;
    constexpr int SO_SNDBUF = 7;
    constexpr int SO_RCVBUF = 8;
    constexpr int SO_KEEPALIVE = 9;
    constexpr int SO_OOBINLINE = 10;
    constexpr int SO_NO_CHECK = 11;
    constexpr int SO_PRIORITY = 12;
    constexpr int SO_LINGER = 13;
    constexpr int SO_BSDCOMPAT = 14;
    constexpr int SO_REUSEPORT = 15;
    constexpr int SO_PASSCRED = 16;
    constexpr int SO_PEERCRED = 17;
    constexpr int SO_RCVLOWAT = 18;
    constexpr int SO_SNDLOWAT = 19;
    constexpr int SO_ACCEPTCONN = 30;
    constexpr int SO_PEERSEC = 31;
    constexpr int SO_SNDBUFFORCE = 32;
    constexpr int SO_RCVBUFFORCE = 33;
    constexpr int SO_PROTOCOL = 38;
    constexpr int SO_DOMAIN = 39;
    constexpr int SO_RCVTIMEO = 20;
    constexpr int SO_SNDTIMEO = 21;
    constexpr int SO_TIMESTAMP = 29;
    constexpr int SO_TIMESTAMPNS = 35;
    constexpr int SO_TIMESTAMPING = 37;
    constexpr int SO_SECURITY_AUTHENTICATION = 22;
    constexpr int SO_SECURITY_ENCRYPTION_TRANSPORT = 23;
    constexpr int SO_SECURITY_ENCRYPTION_NETWORK = 24;
    constexpr int SO_BINDTODEVICE = 25;
    constexpr int SO_ATTACH_FILTER = 26;
    constexpr int SO_DETACH_FILTER = 27;
    constexpr int SO_GET_FILTER = SO_ATTACH_FILTER;
    constexpr int SO_PEERNAME = 28;
    constexpr int SCM_TIMESTAMP = SO_TIMESTAMP;
    constexpr int SO_PASSSEC = 34;
    constexpr int SCM_TIMESTAMPNS = SO_TIMESTAMPNS;
    constexpr int SO_MARK = 36;
    constexpr int SCM_TIMESTAMPING = SO_TIMESTAMPING;
    constexpr int SO_RXQ_OVFL = 40;
    constexpr int SO_WIFI_STATUS = 41;
    constexpr int SCM_WIFI_STATUS = SO_WIFI_STATUS;
    constexpr int SO_PEEK_OFF = 42;
    constexpr int SO_NOFCS = 43;
    constexpr int SO_LOCK_FILTER = 44;
    constexpr int SO_SELECT_ERR_QUEUE = 45;
    constexpr int SO_BUSY_POLL = 46;
    constexpr int SO_MAX_PACING_RATE = 47;
    constexpr int SO_BPF_EXTENSIONS = 48;
    constexpr int SO_INCOMING_CPU = 49;
    constexpr int SO_ATTACH_BPF = 50;
    constexpr int SO_DETACH_BPF = SO_DETACH_FILTER;
    constexpr int SO_ATTACH_REUSEPORT_CBPF = 51;
    constexpr int SO_ATTACH_REUSEPORT_EBPF = 52;
    constexpr int SO_CNX_ADVICE = 53;
    constexpr int SCM_TIMESTAMPING_OPT_STATS = 54;
    constexpr int SO_MEMINFO = 55;
    constexpr int SO_INCOMING_NAPI_ID = 56;
    constexpr int SO_COOKIE = 57;
    constexpr int SCM_TIMESTAMPING_PKTINFO = 58;
    constexpr int SO_PEERGROUPS = 59;
    constexpr int SO_ZEROCOPY = 60;
    constexpr int SO_TXTIME = 61;
    constexpr int SCM_TXTIME = SO_TXTIME;
    constexpr int SO_BINDTOIFINDEX = 62;
    constexpr int SO_DETACH_REUSEPORT_BPF = 68;

    constexpr int SOL_SOCKET = 1;
    constexpr int SOL_IP = 0;
    constexpr int SOL_IPV6 = 41;
    constexpr int SOL_ICMPV6 = 58;
    constexpr int SOL_RAW = 255;
    constexpr int SOL_DECNET = 261;
    constexpr int SOL_X25 = 262;
    constexpr int SOL_PACKET = 263;
    constexpr int SOL_ATM = 264;
    constexpr int SOL_AAL = 265;
    constexpr int SOL_IRDA = 266;
    constexpr int SOL_NETBEUI = 267;
    constexpr int SOL_LLC = 268;
    constexpr int SOL_DCCP = 269;
    constexpr int SOL_NETLINK = 270;
    constexpr int SOL_TIPC = 271;
    constexpr int SOL_RXRPC = 272;
    constexpr int SOL_PPPOL2TP = 273;
    constexpr int SOL_BLUETOOTH = 274;
    constexpr int SOL_PNPIPE = 275;
    constexpr int SOL_RDS = 276;
    constexpr int SOL_IUCV = 277;
    constexpr int SOL_CAIF = 278;
    constexpr int SOL_ALG = 279;
    constexpr int SOL_NFC = 280;
    constexpr int SOL_KCM = 281;
    constexpr int SOL_TLS = 282;
    constexpr int SOL_XDP = 283;
    constexpr int SOMAXCONN = 128;

    constexpr int MSG_OOB = 0x0001;
    constexpr int MSG_PEEK = 0x0002;
    constexpr int MSG_DONTROUTE = 0x0004;
    constexpr int MSG_CTRUNC = 0x0008;
    constexpr int MSG_PROXY = 0x0010;
    constexpr int MSG_TRUNC = 0x0020;
    constexpr int MSG_DONTWAIT = 0x0040;
    constexpr int MSG_EOR = 0x0080;
    constexpr int MSG_WAITALL = 0x0100;
    constexpr int MSG_FIN = 0x0200;
    constexpr int MSG_SYN = 0x0400;
    constexpr int MSG_CONFIRM = 0x0800;
    constexpr int MSG_RST = 0x1000;
    constexpr int MSG_ERRQUEUE = 0x2000;
    constexpr int MSG_NOSIGNAL = 0x4000;
    constexpr int MSG_MORE = 0x8000;
    constexpr int MSG_WAITFORONE = 0x10000;
    constexpr int MSG_BATCH = 0x40000;
    constexpr int MSG_ZEROCOPY = 0x4000000;
    constexpr int MSG_FASTOPEN = 0x20000000;
    constexpr int MSG_CMSG_CLOEXEC = 0x40000000;

    using socklen_t = unsigned;
    using sa_family_t = unsigned short;

    struct iovec {
        void *iov_base;
        size_t iov_len;
    };

    struct msghdr {
        void *msg_name;
        socklen_t msg_namelen;
        iovec *msg_iov;
        size_t msg_iovlen; /* int in POSIX */
        void *msg_control;
        size_t msg_controllen; /* socklen_t in POSIX */
        int msg_flags;
    };
    
    struct sockaddr_storage {
        sa_family_t ss_family;
        char __padding[128 - sizeof(sa_family_t) - sizeof(long)];
        long __force_alignment;
    };
    
    struct mmsghdr {
        struct msghdr msg_hdr;
        unsigned int  msg_len;
    };
    
    struct cmsghdr {
        size_t cmsg_len; /* socklen_t in POSIX */
        int cmsg_level;
        int cmsg_type;
    };

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

    struct route {
        uint32_t dest;
        uint32_t gateway;
        uint32_t netmask;
        uint16_t mtu;

        route(uint32_t dest, uint32_t gateway, uint32_t netmask, uint16_t mtu): dest(dest), gateway(gateway), netmask(netmask), mtu(mtu) {};
    };
    
    struct checksum {
        private:
            uint32_t state = 0;
        public:
            void update(uint16_t word);
            void update(const void *buf, size_t size);

            uint16_t finalize();
    };
}

#endif