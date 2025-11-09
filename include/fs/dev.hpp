#ifndef DEVFS_HPP
#define DEVFS_HPP

#include "frg/string.hpp"
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
            static constexpr size_t BLKRRPART = 0x125F;

            struct device {
                struct partition {
                    size_t blocks;
                    size_t begin;
                    partition(size_t blocks, size_t begin) : blocks(blocks), begin(begin) { };
                };

                ssize_t major;
                ssize_t minor;

                node *file;

                static constexpr size_t BLOCK_IO_READ = 0x229;
                static constexpr size_t BLOCK_IO_WRITE = 0x228;
                bool blockdev;
                struct block_zone {
                    void *buf;
                    size_t len;
                    size_t offset;

                    bool is_success;
                    block_zone *next;
                };

                struct {
                    size_t blocks;
                    size_t block_size;
                    frg::vector<partition, memory::mm::heap_allocator> part_list;

                    ipc::mailbox *mail;

                    void *request_data;
                    void (*request_io)(void *request_data, block_zone *zones, size_t num_zones, size_t part_offset, bool rw);                    
                } block;
            
                bool resolveable;

                device() : major(-1), minor(-1), blockdev(false), block(), resolveable(true) { };
                virtual ~device() { };

                virtual ssize_t on_open(vfs::fd *fd, ssize_t flags) {
                    return -error::NOSYS;
                }

                virtual ssize_t on_close(vfs::fd *fd, ssize_t flags) {
                    return -error::NOSYS;
                }

                virtual ssize_t read(void *buf, size_t len, size_t offset) {
                    return -error::NOSYS;
                }

                virtual ssize_t write(void *buf, size_t len, size_t offset) {
                    return -error::NOSYS;
                }

                virtual ssize_t ioctl(size_t req, void *buf) {
                    return -error::NOSYS;
                }

                virtual void *mmap(node *file, void *addr, size_t len, size_t offset) {
                    return nullptr;
                }
            };

            struct dev_priv {
                device *dev;
                int part;
            };

            static void init();
            static void add(frg::string_view path, device *dev);

            devfs() { };

            node *lookup(node *parent, frg::string_view name) override;

            ssize_t on_open(vfs::fd *fd, ssize_t flags) override;
            ssize_t on_close(vfs::fd *fd, ssize_t flags) override;
            
            bool request_io(node *file, device::block_zone *zones, size_t num_zones, bool rw, bool all_success);
            ssize_t read(node *file, void *buf, size_t len, size_t offset) override;
            ssize_t write(node *file, void *buf, size_t len, size_t offset) override;
            ssize_t ioctl(node *file, size_t req, void *buf) override;
            void *mmap(node *file, void *addr, size_t len, size_t offset) override;
            ssize_t mkdir(node *dst, frg::string_view name, int64_t flags) override;
    };
};

#endif