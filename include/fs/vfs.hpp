#ifndef VFS_HPP
#define VFS_HPP

#include <cstddef>
#include <cstdint>
#include <frg/allocation.hpp>
#include <frg/hash.hpp>
#include <frg/hash_map.hpp>
#include <frg/string.hpp>
#include <frg/vector.hpp>
#include <mm/mm.hpp>
#include <util/lock.hpp>
#include <util/log/log.hpp>
#include <util/shared.hpp>

namespace sched {
    class process;
}

namespace vfs {
    struct node;
    class filesystem;
    class manager;

    using ssize_t = signed long long int;
    using path = frg::string<memory::mm::heap_allocator>;
    using pathlist = frg::vector<frg::string_view, memory::mm::heap_allocator>;
    using nodelist = frg::vector<node *, memory::mm::heap_allocator>;
    
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

    struct error {
        enum {
            INVAL = 1,
            ISDIR,
            NOENT,
            BUSY,
            NOTEMPTY,
            EXIST,
            XDEV,
            IO,
            NOTDIR,
            BADF,
            NOSYS,
            NOFD
        };
    };

    struct oflags {
        enum {
            CREAT = 1,
            APPEND,
            CLOEXEC,
            EXCL,
            NONBLOCK,
            SYNC,
            NOFD,
            NOCTTY
        };
    };

    struct mode {
        enum {
            RDONLY,
            WRONLY,
            RDWR
        };
    };

    struct mflags {
        enum {
            NOSRC = 0x8,
            NODST = 0x4,
        };
    };

    struct sflags {
        enum {
            SET,
            CUR,
            END
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
            FAT,
            EXT
        };
    };

    struct fd;
    struct node {
        public:
            struct type {
                enum {
                    FILE = 1,
                    DIRECTORY,
                    BLOCKDEV,
                    CHARDEV,
                    SOCKET,
                    SYMLINK
                };
            };

            struct statinfo {
                size_t st_dev;
                size_t st_ino;
                size_t st_nlink;
                size_t st_uid;
                size_t st_gid;
                size_t st_rdev;
                size_t st_size;
                size_t st_blksize;
                size_t st_blocks;

                size_t st_atim;
                size_t st_mtim;
                size_t st_ctom;
            };

            void set_fs(filesystem *fs) {
                this->fs = fs;
            } 

            filesystem *get_fs() {
                return fs;
            }

            ssize_t get_type() {
                return type;
            }

            node(filesystem *fs, path name, node *parent, ssize_t flags, ssize_t type, ssize_t inum = -1) : ref_count(), fs(fs), name(name),
            resolveable(true), delete_on_close(false), parent(parent), children(), flags(flags), type(type) {
                if (inum > 0) {
                    this->inum = inum;
                } else {
                    this->inum = parent ? parent->inum++ : 0;
                }

                this->meta = frg::construct<statinfo>(memory::mm::heap);
            };

            vfs::node *find_child(frg::string_view name) {
                for (size_t i = 0; i < children.size(); i++) {
                    if (children[i] == nullptr) continue;
                    if (children[i]->name.eq(name)) return children[i];
                }

                return nullptr;
            }

            ssize_t get_ccount() {
                return children.size();
            }

            void set_parent(node *parent) {
                this->parent = parent;
            }

            node *get_parent() {
                return parent;
            }

            path get_name() {
                return name;
            }

            statinfo *stat() {
                return meta;
            }

            size_t ref_count;
            filesystem *fs;
            statinfo *meta;
            path name;

            bool resolveable;
            bool delete_on_close;

            node *parent;
            void *private_data;
            nodelist children;

            ssize_t inum;
            ssize_t flags;
            ssize_t type;
    };

    class filesystem {
        public:
            node *root;
            node *source;
            nodelist nodes;
            path relpath;

            filesystem() {}

            // TODO: destructor for umount

            void init_as_root(node *root) {
                this->root = root;
                this->source = nullptr;
            }

            virtual void init_fs(node *root, node *source) {
                this->root = root;
                this->source = source;
            }

            virtual node *lookup(node *parent, frg::string_view name) {
                return nullptr;
            }

            virtual ssize_t lsdir(node *dir) {
                return -error::NOSYS;
            }

            virtual ssize_t on_open(vfs::fd *fd, ssize_t flags) {
                return -error::NOSYS;
            }

            virtual ssize_t on_close(vfs::fd *fd, ssize_t flags) {
                return -error::NOSYS;
            }

            virtual ssize_t read(node *file, void *buf, size_t len, size_t offset) {
                return -error::NOSYS;
            }

            virtual void *mmap(node *file, void *addr, size_t len, size_t offset) {
                return nullptr;
            }

            virtual ssize_t write(node *file, void *buf, size_t len, size_t offset) {
                return -error::NOSYS;
            }

            virtual ssize_t ioctl(node *file, size_t req, void *buf) {
                return -error::NOSYS;
            }

            virtual ssize_t create(node *dst, path name, int64_t type, int64_t flags) {
                return -error::NOSYS;
            }

            virtual ssize_t mkdir(node *dst, frg::string_view name, int64_t flags) {
                return -error::NOSYS;
            }

            virtual ssize_t rename(node *src, node *dst, frg::string_view name, int64_t flags) {
                return -error::NOSYS;
            }

            virtual ssize_t link(node *src, node *dst, frg::string_view name, bool is_symlink) {
                return -error::NOSYS;
            }

            virtual ssize_t unlink(node *dst) {
                return -error::NOSYS;
            }

            virtual ssize_t remove(node *dst) {
                return -error::NOSYS;
            }
    };

    // TODO: sockets
    struct pipe;
    struct descriptor {
        util::lock lock;

        vfs::node *node;
        vfs::pipe *pipe;

        size_t ref;
        size_t pos;

        node::statinfo *info;
    };

    struct pipe {
        descriptor *read;
        descriptor *write;
        void *buf;
        size_t len;
        bool data_written;
    };

    struct fd;
    struct fd_table;
    using fd_pair = frg::tuple<vfs::fd *, vfs::fd *>;

    struct fd {
        util::lock lock;
        descriptor *desc;
        fd_table *table;
        int fd_number;
        ssize_t flags;
        ssize_t mode;
    };
    
    struct fd_table {
        util::lock lock;
        frg::hash_map<int, fd *, frg::hash<int>, memory::mm::heap_allocator> fd_list;
        size_t last_fd;

        fd_table(): fd_list(frg::hash<int>()) {}
    };

    static size_t zero = 0;
    filesystem *resolve_fs(frg::string_view path, node *base, size_t& symlinks_traversed = zero);
    node *resolve_at(frg::string_view path, node *base, size_t& symlinks_traversed = zero);
    node *get_parent(node *dir, frg::string_view path);

    ssize_t mount(frg::string_view srcpath, frg::string_view dstpath, ssize_t fstype, int64_t flags);
    ssize_t umount(node *dst);

    vfs::fd *open(node *dir, frg::string_view filepath, fd_table *table, int64_t flags, int64_t mode);
    fd_pair open_pipe(fd_table *table, ssize_t flags);
    ssize_t lseek(vfs::fd *fd, size_t off, size_t whence);
    vfs::fd *dup(vfs::fd *fd, ssize_t flags, ssize_t new_num);
    ssize_t close(vfs::fd *fd);
    ssize_t read(vfs::fd *fd, void *buf, size_t len);
    ssize_t write(vfs::fd *fd, void *buf, size_t len);
    ssize_t ioctl(vfs::fd *fd, size_t req, void *buf);
    void *mmap(vfs::fd *fd, void *addr, size_t off, size_t len);
    ssize_t lstat(node *dir, frg::string_view filepath, node::statinfo *buf);
    ssize_t create(node *dir, frg::string_view filepath, fd_table *table, int64_t type, int64_t flags, int64_t mode);
    ssize_t mkdir(node *dir, frg::string_view dirpath, int64_t flags, int64_t mode);
    ssize_t rename(node *old_dir, frg::string_view oldpath, node *new_dir, frg::string_view newpath, int64_t flags);
    ssize_t link(node *from_dir, frg::string_view from, node *to_dir, frg::string_view to, bool is_symlink);
    ssize_t unlink(node *dir, frg::string_view filepath);
    ssize_t rmdir(node *dir, frg::string_view dirpath);
    pathlist lsdir(node *dir, frg::string_view dirpath);

    // fs use only
    vfs::fd *make_fd(vfs::node *node, fd_table *table, int64_t flags, int64_t mode);
    vfs::node *make_node(node *dst, frg::string_view path, int64_t type);
    filesystem *device_fs();

    // sched use
    fd_table *make_table();
    fd_table *copy_table(fd_table *table);
    void delete_table(fd_table *table);

    void init();
};

#endif