#ifndef DEVFS_HPP
#define DEVFS_HPP

#include <cstddef>
#include <frg/hash.hpp>
#include <frg/hash_map.hpp>
#include <frg/tuple.hpp>
#include <frg/vector.hpp>
#include <fs/vfs.hpp>
#include <sys/sched/mail.hpp>
#include <mm/mm.hpp>

namespace vfs {
    class devfs : public vfs::filesystem {
        public:
            static constexpr size_t STORAGE_MAJOR = 0xA;

            static constexpr size_t BLKRRPART = 0x125F;
            static constexpr size_t BLKLMODE = 0x126;

            struct device {
                struct partition {
                    ssize_t blocks;
                    ssize_t begin;
                    partition(size_t blocks, size_t begin) : blocks(blocks), begin(begin) { };
                };

                ssize_t major;
                ssize_t minor;

                static constexpr size_t BLOCK_IO_READ = 0x229;
                static constexpr size_t BLOCK_IO_WRITE = 0x228;
                bool blockdev;
                struct block_zone {
                    void *buf;
                    ssize_t len;
                    ssize_t offset;

                    bool is_success;
                    block_zone *next;
                };

                struct {
                    ssize_t blocks;
                    ssize_t block_size;
                    frg::vector<partition, memory::mm::heap_allocator> part_list;

                    ipc::mailbox *mail;

                    void *request_data;
                    void (*request_io)(void *request_data, block_zone *zones, size_t num_zones, ssize_t part_offset, bool rw);
                    
                    bool lmode;
                } block;
            
                bool resolveable;
                vfs::path name;

                device() : major(-1), minor(-1), blockdev(false), block(), resolveable(true) { };
                virtual ~device() { };

                virtual ssize_t on_open(vfs::fd *fd, ssize_t flags) {
                    return -error::NOSYS;
                }

                virtual ssize_t on_close(vfs::fd *fd, ssize_t flags) {
                    return -error::NOSYS;
                }

                virtual ssize_t read(void *buf, ssize_t len, ssize_t offset) {
                    return -error::NOSYS;
                }

                virtual ssize_t write(void *buf, ssize_t len, ssize_t offset) {
                    return -error::NOSYS;
                }

                virtual ssize_t ioctl(size_t req, void *buf) {
                    return -error::NOSYS;
                }

                virtual void *mmap(node *file, void *addr, ssize_t len, ssize_t offset) {
                    return nullptr;
                }
            };

            static void init();
            static void add(device *dev);
            static void rm(ssize_t major, ssize_t minor);
            static void rm(vfs::path name);
            static device *find(ssize_t major, ssize_t minor);
            static device *find(vfs::path name);
            static device *find(node* node);
            
            static inline pathmap<frg::vector<node *, memory::mm::heap_allocator>> node_map{path_hasher()};
            static inline pathmap<device *> device_map{vfs::path_hasher()};

            devfs() { };

            node *lookup(const pathlist& filepath, frg::string_view path, int64_t flags) override;

            ssize_t on_open(vfs::fd *fd, ssize_t flags) override;
            ssize_t on_close(vfs::fd *fd, ssize_t flags) override;
            
            bool request_io(node *file, device::block_zone *zones, size_t num_zones, bool rw, bool all_success);
            ssize_t read(node *file, void *buf, ssize_t len, ssize_t offset) override;
            ssize_t write(node *file, void *buf, ssize_t len, ssize_t offset) override;
            ssize_t ioctl(node *file, size_t req, void *buf) override;
            void *mmap(node *file, void *addr, ssize_t len, ssize_t offset) override;
            ssize_t mkdir(const pathlist& dirpath, int64_t flags) override;
            ssize_t lsdir(node *dir, pathlist& names) override;
    };
};

#endif