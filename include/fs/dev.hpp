#ifndef DEVFS_HPP
#define DEVFS_HPP

#include <cstddef>
#include <frg/hash.hpp>
#include <frg/hash_map.hpp>
#include <frg/tuple.hpp>
#include <frg/vector.hpp>
#include <fs/vfs.hpp>
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
                ssize_t blocks;
                ssize_t block_size;
                frg::vector<partition, memory::mm::heap_allocator> part_list;

                vfs::path name;

                bool lmode;

                device() : major(-1), minor(-1), lmode(false) { };
                virtual ~device() { };

                virtual ssize_t read(void *buf, ssize_t len, ssize_t offset) {
                    return -error::NOSYS;
                }

                virtual ssize_t write(void *buf, ssize_t len, ssize_t offset) {
                    return -error::NOSYS;
                }

                virtual ssize_t ioctl(size_t req, void *buf) {
                    return -error::NOSYS;
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

            ssize_t read(node *file, void *buf, ssize_t len, ssize_t offset) override;
            ssize_t write(node *file, void *buf, ssize_t len, ssize_t offset) override;
            ssize_t ioctl(node *file, size_t req, void *buf) override;
            ssize_t remove(node *dest) override;
            ssize_t lsdir(node *dir, pathlist& names) override;
    };
};

#endif