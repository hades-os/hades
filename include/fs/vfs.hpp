#ifndef VFS_HPP
#define VFS_HPP

#include "driver/net/types.hpp"
#include "fs/poll.hpp"
#include "mm/arena.hpp"
#include "mm/slab.hpp"
#include "smarter/smarter.hpp"
#include <cstddef>
#include <cstdint>
#include <frg/allocation.hpp>
#include <frg/hash.hpp>
#include <frg/hash_map.hpp>
#include <frg/string.hpp>
#include <frg/vector.hpp>
#include <mm/mm.hpp>
#include <sys/sched/time.hpp>
#include <util/lock.hpp>
#include <util/log/log.hpp>
#include <util/types.hpp>
#include <util/errors.hpp>
#include <utility>

namespace sched {
    class process;
    class thread;
}

namespace vfs {
    struct node;
    class filesystem;
    class manager;

    using path = frg::string<prs::allocator>;
    using pathlist = frg::vector<frg::string_view, prs::allocator>;

    struct path_hasher {
        unsigned int operator() (frg::string_view path) {
            return frg::CStringHash{}(path.data());
        }
    };

    inline frg::string_view find_name(frg::string_view path) {
        auto pos = path.find_last('/');
        if (pos == size_t(-1)) {
            return path;
        }

        return path.sub_string(pos + 1);
    }

    struct mflags {
        enum {
            NOSRC = 0x8,
            NODST = 0x4,
        };
    };

    struct fslist {
        enum {
            ROOTFS = 1,
            SYSFS,
            PROCFS,
            DEVFS,
            TMPFS,
            INITRD,
            EXT
        };
    };

    struct fd;
    struct descriptor;

    struct node {
        private:
            void init(ssize_t inum) {
                if (inum > 0) {
                    this->inum = inum;
                } else {
                    if (!parent.expired()) {
                        auto new_parent = parent.lock();
                        this->inum = new_parent ? new_parent->inum++ : 0;
                    }
                }

                this->meta = prs::allocate_shared<statinfo>(mm::slab<statinfo>());
            }

        public:
            struct type {
                enum {
                    FILE = 1,
                    DIRECTORY,
                    BLOCKDEV,
                    CHARDEV,
                    SOCKET,
                    SYMLINK,
                    FIFO
                };
            };

            struct statinfo {
                dev_t st_dev;
                ino_t st_ino;
                mode_t st_mode;
                nlink_t st_nlink;
                uid_t st_uid;
                gid_t st_gid;
                dev_t st_rdev;
                off_t st_size;

                sched::timespec st_atim;
                sched::timespec st_mtim;
                sched::timespec st_ctim;

                blksize_t st_blksize;
                blkcnt_t st_blkcnt;
            };

            node(weak_ptr<filesystem> fs, path name, weak_ptr<node> parent, ssize_t flags, ssize_t type, ssize_t inum = -1) : fs(fs), name(std::move(name)),
                delete_on_close(false), parent(parent), allocator(), children(allocator), flags(flags), type(type), lock() {
                init(inum);
            };

            node(std::nullptr_t, path name, std::nullptr_t, ssize_t flags, ssize_t type, ssize_t inum = -1):
                fs(), name(std::move(name)), delete_on_close(false), parent(), allocator(), children(allocator), flags(flags), type(type), lock() {
                init(inum);
            }

            shared_ptr<node> find_child(frg::string_view name) {
                for (size_t i = 0; i < children.size(); i++) {
                    if (children[i]) {
                        if (children[i]->name.eq(name)) return children[i];
                    }
                }

                return {nullptr};
            }

            template<typename T>
            shared_ptr<T> data_as() {
                return prs::reinterpret_pointer_cast<T>(this->data);
            }

            template<typename T>
            void as_data(shared_ptr<T> data) {
                this->data = prs::reinterpret_pointer_cast<void *>(data);
            }

            bool has_access(uid_t uid, gid_t gid, int mode) {
                if (uid == 0) {
                    return true;
                }

                mode_t mask_uid = 0, mask_gid = 0, mask_oth = 0;

                if(mode & R_OK) { mask_uid |= S_IRUSR; mask_gid |= S_IRGRP; mask_oth |= S_IROTH; }
                if(mode & W_OK) { mask_uid |= S_IWUSR; mask_gid |= S_IWGRP; mask_oth |= S_IWOTH; }
                if(mode & X_OK) { mask_uid |= S_IXUSR; mask_gid |= S_IXGRP; mask_oth |= S_IXOTH; }

                if(meta->st_uid == uid) {
                    if((meta->st_mode & mask_uid) == mask_uid) {
                        return true;
                    }

                    return false;
                } else if(meta->st_gid == gid) {
                    if((meta->st_mode & mask_gid) == mask_gid) {
                        return true;
                    }

                    return false;
                } else {
                    if((meta->st_mode & mask_oth) == mask_oth) {
                        return true;
                    }

                    return false;
                }
            }

            weak_ptr<filesystem> fs;
            shared_ptr<statinfo> meta;

            path name;
            path link_target;

            bool delete_on_close;

            weak_ptr<node> parent;

            arena::allocator allocator;
            
            // TODO: lc-rs tree instead of vector
            frg::vector<shared_ptr<node>, arena::allocator> children;

            shared_ptr<void *> data;

            ssize_t inum;
            ssize_t flags;
            ssize_t type;

            util::spinlock lock;
    };

    struct fd;
    struct fd_table;

    class filesystem {
        public:
            shared_ptr<node> root;
            weak_ptr<node> device;

            shared_ptr<filesystem> self;
            path relpath;

            filesystem(shared_ptr<node> root, weak_ptr<node> device):
                root(root), device(device) {}

            virtual bool load() { return true; };

            virtual weak_ptr<node>lookup(shared_ptr<node> parent, frg::string_view name) {
                return {};
            }

            virtual ssize_t readdir(shared_ptr<node> dir) {
                return -ENOTSUP;
            }

            virtual ssize_t on_open(shared_ptr<fd> fd, ssize_t flags) {
                return -ENOTSUP;
            }

            virtual ssize_t on_close(shared_ptr<fd> fd, ssize_t flags) {
                return -ENOTSUP;
            }

            virtual ssize_t read(shared_ptr<node> file, void *buf, size_t len, off_t offset) {
                return -ENOTSUP;
            }

            virtual void *mmap(shared_ptr<node> file, void *addr, size_t len, off_t offset) {
                return nullptr;
            }

            virtual ssize_t write(shared_ptr<node> file, void *buf, size_t len, off_t offset) {
                return -ENOTSUP;
            }

            virtual ssize_t truncate(shared_ptr<node> file, off_t offset) {
                return 0;
            }

            virtual ssize_t ioctl(shared_ptr<node> file, size_t req, void *buf) {
                return -ENOTSUP;
            }

            virtual ssize_t poll(shared_ptr<descriptor> file) {
                return -ENOTSUP;
            }

            virtual ssize_t create(shared_ptr<node> dst, path name, int64_t type, int64_t flags, mode_t mode,
                uid_t uid, gid_t gid) {
                return -ENOTSUP;
            }

            virtual ssize_t mkdir(shared_ptr<node> dst, frg::string_view name, int64_t flags, mode_t mode,
                uid_t uid, gid_t gid) {
                return -ENOTSUP;
            }

            virtual ssize_t rename(shared_ptr<node> src, shared_ptr<node> dst, frg::string_view name, int64_t flags) {
                return -ENOTSUP;
            }

            virtual ssize_t readlink(shared_ptr<node> file) {
                return -ENOTSUP;
            }

            virtual ssize_t link(shared_ptr<node> src, shared_ptr<node> dst, frg::string_view name, bool is_symlink) {
                return -ENOTSUP;
            }

            virtual ssize_t unlink(shared_ptr<node> dst) {
                return -ENOTSUP;
            }

            virtual ssize_t remove(shared_ptr<node> dst) {
                return -ENOTSUP;
            }
    };

    struct network;
    struct socket;

    struct network {
        shared_ptr<network> self;
        
        virtual shared_ptr<socket> create(int type, int protocol) {
            return {};
        };

        virtual ssize_t close(shared_ptr<socket> socket) {
            return -ENOTSUP;
        }

        virtual ssize_t poll(shared_ptr<descriptor> file) {
            return -ENOTSUP;
        }

        virtual ssize_t sockopt(shared_ptr<socket> socket, bool set, int level, int optname, void *optval) {
            return -ENOTSUP;
        }

        virtual ssize_t bind(shared_ptr<socket> socket, net::sockaddr_storage *addr, net::socklen_t addr_len) {
            return -ENOTSUP;
        }

        virtual ssize_t listen(shared_ptr<socket> socket, int backlog) {
            return -ENOTSUP;
        }

        virtual shared_ptr<socket> accept(shared_ptr<socket> socket, net::sockaddr_storage *addr, net::socklen_t addr_len, int flags) {
            return {};
        }

        virtual ssize_t connect(shared_ptr<socket> socket, net::sockaddr_storage *addr, net::socklen_t addr_len) {
            return -ENOTSUP;
        }

        virtual ssize_t shutdown(shared_ptr<socket> socket, int how) {
            return -ENOTSUP;
        }

        virtual ssize_t sendto(shared_ptr<socket> socket, void *buf, size_t len, net::sockaddr_storage *addr, net::socklen_t addr_len, int flags) {
            return  -ENOTSUP;
        }

        virtual ssize_t sendmsg(shared_ptr<socket> socket, net::msghdr *hdr, int flags) {
            return -ENOTSUP;
        }

        virtual ssize_t recvfrom(shared_ptr<socket> socket, void *buf, size_t len, net::sockaddr_storage *addr, net::socklen_t addr_len, int flags) {
            return  -ENOTSUP;
        }

        virtual ssize_t recvmsg(shared_ptr<socket> socket, net::msghdr *hdr, int flags) {
            return -ENOTSUP;
        }
    };

    struct socket {
        weak_ptr<vfs::network> network;
        shared_ptr<node> fs_node;

        path name;
        path peername;

        shared_ptr<void *> data;
        util::spinlock lock;

        prs::allocator allocator;

        socket(weak_ptr<vfs::network> network):
            network(network), fs_node(),
            name(), peername(),
            lock(), allocator() {}

        template<typename T>
        shared_ptr<T> data_as() {
            return prs::reinterpret_pointer_cast<T>(this->data);
        }

        template<typename T>
        void as_data(shared_ptr<T> data) {
            this->data = prs::reinterpret_pointer_cast<void *>(data);
        }
    };

    // TODO: sockets
    struct pipe;
    struct descriptor {
        shared_ptr<vfs::node> node;
        shared_ptr<vfs::pipe> pipe;
        shared_ptr<vfs::socket> socket;

        size_t ref;
        size_t pos;

        shared_ptr<node::statinfo> info;
        shared_ptr<poll::producer> producer;

        prs::allocator allocator;

        int current_ent;
        frg::vector<dirent *, prs::allocator> dirent_list;

        descriptor(): allocator(slab::create_resource<dirent>()), dirent_list(allocator) {}
    };

    struct pipe {
        shared_ptr<descriptor> read;
        shared_ptr<descriptor> write;

        prs::allocator allocator;

        void *buf;
        size_t len;

        bool data_written;
    };

    using fd_pair = frg::tuple<shared_ptr<fd>, shared_ptr<fd>>;
    struct fd {
        util::spinlock lock;
        shared_ptr<descriptor> desc;
        weak_ptr<fd_table> table;
        int fd_number;
        ssize_t flags;
        ssize_t mode;

        fd(): lock() {};
    };

    struct fd_table {
        util::spinlock lock;
        prs::allocator allocator;
        frg::hash_map<
            int, shared_ptr<fd>,
            frg::hash<int>, prs::allocator
        > fd_list;
        size_t last_fd;

        fd_table(): lock(), allocator(arena::create_resource()), fd_list(frg::hash<int>(), allocator) {}
    };

    static size_t zero = 0;
    weak_ptr<filesystem> resolve_fs(frg::string_view path, shared_ptr<node> base, size_t& symlinks_traversed = zero);
    shared_ptr<node> resolve_at(frg::string_view path, shared_ptr<node> base, bool follow_symlink = true, size_t& symlinks_traversed = zero);
    path get_abspath(shared_ptr<node> node);
    weak_ptr<node> get_parent(shared_ptr<node> base, frg::string_view path);

    ssize_t mount(frg::string_view srcpath, frg::string_view dstpath, ssize_t fstype, int64_t flags);
    ssize_t umount(shared_ptr<node> dst);

    shared_ptr<fd> open(shared_ptr<node> base, frg::string_view filepath, shared_ptr<fd_table> table, int64_t flags, mode_t mode,
        uid_t uid, gid_t gid);
    fd_pair open_pipe(shared_ptr<fd_table> table, ssize_t flags);

    ssize_t lseek(shared_ptr<fd> fd, off_t off, size_t whence);
    shared_ptr<fd> dup(shared_ptr<fd> fd, bool cloexec, ssize_t new_num);
    ssize_t close(shared_ptr<fd> fd);

    ssize_t read(shared_ptr<fd> fd, void *buf, size_t len);
    ssize_t write(shared_ptr<fd> fd, void *buf, size_t len);
    ssize_t pread(shared_ptr<fd> fd, void *buf, size_t len, off_t offset);
    ssize_t pwrite(shared_ptr<fd> fd, void *buf, size_t len, off_t offset);

    ssize_t ioctl(shared_ptr<fd> fd, size_t req, void *buf);
    void *mmap(shared_ptr<fd> fd, void *addr, off_t off, size_t len);

    ssize_t poll(pollfd *fds, nfds_t nfds, shared_ptr<fd_table> table, sched::timespec *timespec);

    ssize_t stat(shared_ptr<node> dir, frg::string_view filepath, node::statinfo *buf, int64_t flags);

    ssize_t create(shared_ptr<node> base, frg::string_view filepath, shared_ptr<fd_table> table, int64_t type, int64_t flags, mode_t mode,
        uid_t uid, gid_t gid);
    ssize_t mkdir(shared_ptr<node> base, frg::string_view dirpath, int64_t flags, mode_t mode,
        uid_t uid, gid_t gid);

    ssize_t rename(shared_ptr<node> old_base, frg::string_view oldpath, shared_ptr<node> new_base, frg::string_view newpath, int64_t flags);
    ssize_t link(shared_ptr<node> from_base, frg::string_view from, shared_ptr<node> to_base, frg::string_view to, bool is_symlink);
    ssize_t unlink(shared_ptr<node> base, frg::string_view filepath);
    ssize_t rmdir(shared_ptr<node> base, frg::string_view dirpath);
    pathlist readdir(shared_ptr<node> base, frg::string_view dirpath);

    shared_ptr<fd> create_socket(shared_ptr<network> network, int type, int protocol);
    ssize_t close_socket(shared_ptr<fd> fd, int how);
    ssize_t sockopt(shared_ptr<fd> fd, bool set, int level, int optname, void *optval);
    ssize_t bind(shared_ptr<fd> fd, net::sockaddr_storage *addr, net::socklen_t addr_len);
    ssize_t listen(shared_ptr<fd> fd, int backlog);
    ssize_t accept(shared_ptr<fd> fd, net::sockaddr_storage *addr, net::socklen_t addr_len, int flags);
    ssize_t connect(shared_ptr<fd> fd, net::sockaddr_storage *addr, net::socklen_t addr_len);

    ssize_t sendto(shared_ptr<fd> fd, void *buf, size_t len, net::sockaddr_storage *addr, net::socklen_t addr_len, int flags);
    ssize_t sendmsg(shared_ptr<fd> fd, net::msghdr *hdr, int flags);
    ssize_t recvfrom(shared_ptr<fd> fd, void *buf, size_t len, net::sockaddr_storage *addr, net::socklen_t addr_len, int flags);
    ssize_t recvmsg(shared_ptr<fd> fd, net::msghdr *hdr, int flags);

    // fs use only
    mode_t type2mode(int64_t type);

    shared_ptr<fd> make_fd(shared_ptr<node> node, shared_ptr<fd_table> table, int64_t flags, mode_t mode);
    shared_ptr<node> make_recursive(shared_ptr<node> base, frg::string_view path, int64_t type, mode_t mode);
    shared_ptr<filesystem> device_fs();

    // sched use
    shared_ptr<fd_table>  make_table();
    shared_ptr<fd_table>  copy_table(shared_ptr<fd_table> table);

    extern shared_ptr<node> tree_root;
    void init();
};

#endif