#ifndef UNIX_HPP
#define UNIX_HPP

#include <cstddef>
#include <driver/net/types.hpp>

namespace net {
    struct sockaddr_un {
        sa_family_t sun_family;
        char sun_path[108];
    };
    
    // Evaluate to actual length of the `sockaddr_un' structure.
    inline size_t SUN_LEN(sockaddr_un *p) {
        return offsetof(struct sockaddr_un, sun_path) + strlen(p->sun_path);
    }
}

#endif