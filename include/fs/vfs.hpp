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

namespace vfs {
    class node;
    class filesystem;
    class manager;

    using ssize_t = signed long long int;
    using path = frg::string<memory::mm::heap_allocator>;
    using pathlist = frg::vector<path, memory::mm::heap_allocator>;
    using nodelist = frg::vector<node *, memory::mm::heap_allocator>;
    using optlist = pathlist;

    struct path_hasher {
        unsigned int operator() (vfs::path path) {
            return frg::CStringHash{}(path.data());
        }
    };

    template<typename T>
    using pathmap = frg::hash_map<path, T, path_hasher, memory::mm::heap_allocator>;

    inline vfs::path find_name(vfs::path path) {
        auto view = frg::string_view(path);
        auto pos = view.find_last('/');
        if (pos == -1) {
            return view;
        }

        return view.sub_string(pos + 1);
    }

    inline pathlist split_path(frg::string_view path) {
        pathlist components;
        auto view = path;

        ssize_t next_slash;
        while ((next_slash = view.find_first('/')) != -1) {
            auto pos = next_slash == -1 ? view.size() : next_slash;

            if (auto c = view.sub_string(0, pos); c.size())
                components.push_back(c);

            view = view.sub_string(pos + 1);
        }

        components.push_back(view.data() + view.find_last('/') + 1);
        return components;
    }

    inline void append_paths(vfs::path& path, vfs::path part) {
        path += "/";
        path += part;
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
            NOFD
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
            OVERLAY = 0x10
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

    class node {
        public:
            struct type {
                enum {
                    FILE = 1,
                    DIRECTORY,
                    BLOCKDEV,
                    CHARDEV,
                    SOCKET,
                    HARDLINK,
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

            node(filesystem *fs, path name, path abspath, node *parent, ssize_t flags, ssize_t type) : ref_count(), fs(fs), name(name),
                abspath(abspath), parent(parent), children(path_hasher()), links(path_hasher()), flags(flags), type(type) {
                if (parent) {
                    this->inum = parent->inum + 1;
                    parent->children[abspath] = this;
                } else {
                    this->inum = 0;
                }

                this->meta = frg::construct<statinfo>(memory::mm::heap);
                this->master = nullptr;
                this->link = nullptr;
            };

            void add_child(node *child) {
                children[child->abspath] = child;
            }

            void rm_child(path abspath) {
                children.remove(abspath);
            }

            void add_link(node *link) {
                links[link->abspath] = link;
            }

            void rm_link(path lpath) {
                for (size_t i = 0; i < links.size(); i++) {
                    auto link = links[i];
                    if (link->abspath == lpath) {
                        links.remove(i);
                    }
                }
            }

            void set_master(node *new_master) {
                new_master->add_link(this);
                master->rm_link(this->abspath);
                this->master = new_master;
                *this->link = new_master->abspath;
            }

            void clear_master() {
                if (!master) {
                    return;
                }

                master->rm_link(this->abspath);
                this->master = nullptr;
                this->link = nullptr;
            }

            void remove_master() {
                if (!master) {
                    return;
                }

                master->rm_link(this->abspath);
                this->master = nullptr;
            }

            void clear_links() {
                for (auto& [abspath, link] : links) {
                    link->remove_master();
                }
            }

            void set_link(vfs::path link) {
                *this->link = link;
            }

            void clear_link() {
                this->link = nullptr;
            }

            vfs::path *get_link() {
                return link;
            }

            node *get_master() {
                return master;
            }

            ssize_t get_ccount() {
                return children.size();
            }

            void set_path(path abspath) {
                this->abspath = abspath;
            }

            path& get_path() {
                return abspath;
            }

            pathmap<node *> *get_children() {
                return &children;
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
        private:
            filesystem *fs;
            statinfo *meta;
            path name;
            path abspath;
            path *link;

            node *parent;
            node *master;
            pathmap<node *> children;
            pathmap<node *> links;

            ssize_t inum;
            ssize_t flags;
            ssize_t type;
    };

    class filesystem {
        protected:
            node *root;
            node *source;
            nodelist nodes;
            path relpath;
        public:
            filesystem() : nodenames(path_hasher()) {}

            // TODO: fix destructor
            virtual ~filesystem() {
                if (root->get_parent()) {
                    root->get_parent()->rm_child(root->get_path());
                }
                
                frg::destruct(memory::mm::heap, root);
                for (auto node : nodes) {
                    node->clear_links();
                    frg::destruct(memory::mm::heap, node);
                }
            }

            pathmap<node *> nodenames;

            path abspath;

            void init_as_root(node *root) {
                this->root = root;
                this->source = nullptr;
                this->nodenames[root->get_path()] = root;
                this->abspath = "/";
            }

            virtual void init_fs(node *root, node *source) {
                this->root = root;
                this->source = source;
                this->nodenames[root->get_path()] = root;
                this->abspath = root->get_path();
            }

            virtual node *lookup(const pathlist& filepath, frg::string_view path, int64_t flags) {
                return nullptr;
            }

            virtual ssize_t read(node *file, void *buf, ssize_t len, ssize_t offset) {
                return -error::NOSYS;
            }

            virtual ssize_t mmap(node *file, void *addr, ssize_t len, ssize_t offset) {
                return -error::NOSYS;
            }

            virtual ssize_t write(node *file, void *buf, ssize_t len, ssize_t offset) {
                return -error::NOSYS;
            }

            virtual ssize_t ioctl(node *file, size_t req, void *buf) {
                return -error::NOSYS;
            }

            virtual ssize_t create(path name, node *parent, node *nnode, int64_t type, int64_t flags) {
                return -error::NOSYS;
            }

            virtual ssize_t mkdir(const pathlist& dirpath, int64_t flags) {
                return -error::NOSYS;
            }

            virtual ssize_t rename(const pathlist& oldpath, const pathlist& newpath, int64_t flags) {
                return -error::NOSYS;
            }

            virtual ssize_t link(const pathlist& from, const pathlist& to, bool is_symlink) {
                return -error::NOSYS;
            }

            virtual ssize_t remove(node *dest) {
                return -error::NOSYS;
            }

            virtual ssize_t rmdir(node *dir) {
                return -error::NOSYS;
            }

            virtual ssize_t lsdir(node *dir, vfs::pathlist& names) {
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

        ssize_t pos;
        ssize_t flags;
        ssize_t mode;

        node::statinfo *info;
    };

    struct pipe {
        descriptor *read;
        descriptor *write;
        void *buf;
    };

    struct fd;
    struct fd_table;

    struct fd {
        util::lock lock;
        descriptor *desc;
        fd_table *table;
        int fd_number;
        int flags;
    };
    
    struct fd_table {
        util::lock lock;
        frg::hash_map<int, fd *, frg::hash<int>, memory::mm::heap_allocator> fd_list;
        size_t last_fd;

        fd_table(): fd_list(frg::hash<int>()) {}
    };

    node *resolve(frg::string_view path);
    node *get_parent(frg::string_view path);
    filesystem *resolve_fs(frg::string_view path);

    ssize_t mount(frg::string_view srcpath, frg::string_view dstpath, ssize_t fstype, optlist *opts, int64_t flags);
    ssize_t umount(node *dst);

    vfs::fd *open(frg::string_view filepath, fd_table *table, int64_t flags, int64_t mode);
    ssize_t lseek(vfs::fd *fd, size_t off, size_t whence);
    ssize_t close(vfs::fd *fd);
    ssize_t read(vfs::fd *fd, void *buf, ssize_t len);
    ssize_t map(vfs::fd *fd, void *addr, ssize_t off, ssize_t len);
    ssize_t write(vfs::fd *fd, void *buf, ssize_t len);
    ssize_t ioctl(vfs::fd *fd, size_t req, void *buf);
    ssize_t lstat(frg::string_view filepath, node::statinfo *buf);
    ssize_t create(frg::string_view filepath, fd_table *table, int64_t type, int64_t flags, int64_t mode);
    ssize_t mkdir(frg::string_view dirpath, int64_t flags, int64_t mode);
    ssize_t rename(frg::string_view oldpath, frg::string_view newpath, int64_t flags);
    ssize_t link(frg::string_view from, frg::string_view to, bool is_symlink);
    ssize_t unlink(frg::string_view filepath);
    ssize_t rmdir(frg::string_view dirpath);
    pathlist lsdir(frg::string_view dirpath);

    // fs use only
    ssize_t in_use(frg::string_view filepath);
    ssize_t insert_node(frg::string_view filepath, int64_t type);

    // sched use
    fd_table *make_table();
    fd_table *copy_table(fd_table *table);
    void delete_table(fd_table *table);

    void init();
};

#endif