#include <driver/net/unix.hpp>
#include <fs/vfs.hpp>
#include <util/types.hpp>
#include <driver/net/types.hpp>
#include <mm/mm.hpp>
#include <smarter/smarter.hpp>
#include "frg/string.hpp"
#include "mm/slab.hpp"

/**
        socket(weak_ptr<vfs::network> network, shared_ptr<node> fs_node,
            bool delete_on_close, path name, path peername):
            network(network), fs_node(fs_node),
            delete_on_close(delete_on_close), name(std::move(name)), peername(std::move(peername)),
            lock() {}
*/

shared_ptr<vfs::socket> net::unix::create(int type, int protocol) {
    if (protocol != 0) return {};
    switch (type) {
        case SOCK_DGRAM:;
        case SOCK_STREAM:;
        case SOCK_SEQPACKET:
            auto socket = prs::allocate_shared<vfs::socket>(mm::slab<vfs::socket>(),
                self);
            auto data = prs::allocate_shared<unix::data>(socket->allocator);
            socket->as_data(data);

            return socket;
    }

    return {};
}

ssize_t net::unix::bind(shared_ptr<vfs::socket> socket, net::sockaddr_storage *addr, net::socklen_t addr_len) {
    sockaddr_un *unix_addr = (sockaddr_un *) addr;
    if (unix_addr->sun_path[0] == '\0') {
        char *name = unix_addr->sun_path + 1;
        frg::string_view abstract_name{name, addr_len - sizeof(sa_family_t) - 1};

        if (abstract_names.contains(abstract_name)) return -EADDRINUSE;

        socket->name = abstract_name;
        abstract_names.insert(abstract_name, socket);

        return 0;
    } else {
        char *name = unix_addr->sun_path;
        frg::string_view path_name{name, strnlen(name, 108)};

        if (vfs::resolve_at(path_name, nullptr)) {
            return -EADDRINUSE;
        }

        auto parent = vfs::get_parent(nullptr, path_name);
        if (parent.expired()) {
            return -ENOENT;
        }

        auto node = vfs::make_recursive(nullptr, path_name, vfs::node::type::SOCKET, DEFAULT_MODE);
        if (!node) {
            return -ENOENT;
        }

        socket->name = path_name;
        socket->fs_node = node;

        return 0;
    }
}

ssize_t net::unix::listen(shared_ptr<vfs::socket> socket, int backlog) {
    
}