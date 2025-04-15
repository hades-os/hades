#ifndef UNIX_HPP
#define UNIX_HPP

#include <cstddef>
#include <driver/net/types.hpp>
#include "frg/hash_map.hpp"
#include "frg/string.hpp"
#include "fs/vfs.hpp"
#include "ipc/wire.hpp"

namespace net {
    struct sockaddr_un {
        sa_family_t sun_family;
        char sun_path[108];
    };
    
    // Evaluate to actual length of the `sockaddr_un' structure.
    inline size_t SUN_LEN(sockaddr_un *p) {
        return offsetof(struct sockaddr_un, sun_path) + strlen(p->sun_path);
    }

    class unix: vfs::network {
        private:
            frg::hash_map<
                frg::string_view,
                weak_ptr<vfs::socket>,
                vfs::path_hasher,
                memory::mm::heap_allocator
            > abstract_names;

            enum state {
                CLOSED,
                LISTEN
            };

            struct data {
                size_t state;
                ipc::wire wire;

                

                data(): state(state::CLOSED), wire() {}
            };
        public:
            shared_ptr<vfs::socket> create(int type, int protocol) override;
            ssize_t close(shared_ptr<vfs::socket> socket) override;
            ssize_t poll(shared_ptr<vfs::descriptor> file) override;
            ssize_t sockopt(shared_ptr<vfs::socket> socket, bool set, int level, int optname, void *optval) override;
            ssize_t bind(shared_ptr<vfs::socket> socket, net::sockaddr_storage *addr, net::socklen_t addr_len) override;
            ssize_t listen(shared_ptr<vfs::socket> socket, int backlog) override;
            shared_ptr<vfs::socket> accept(shared_ptr<vfs::socket> socket, net::sockaddr_storage *addr, net::socklen_t addr_len, int flags) override;
            ssize_t connect(shared_ptr<vfs::socket> socket, net::sockaddr_storage *addr, net::socklen_t addr_len) override;
            ssize_t shutdown(shared_ptr<vfs::socket> socket, int how) override;
            ssize_t sendto(shared_ptr<vfs::socket> socket, void *buf, size_t len, net::sockaddr_storage *addr, net::socklen_t addr_len, int flags) override;
            ssize_t sendmsg(shared_ptr<vfs::socket> socket, net::msghdr *hdr, int flags) override;
            ssize_t recvfrom(shared_ptr<vfs::socket> socket, void *buf, size_t len, net::sockaddr_storage *addr, net::socklen_t addr_len, int flags) override;
            ssize_t recvmsg(shared_ptr<vfs::socket> socket, net::msghdr *hdr, int flags) override;

        unix(): abstract_names(vfs::path_hasher()) {}
    };
}

#endif