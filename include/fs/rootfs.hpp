#ifndef ROOTFS_HPP
#define ROOTFS_HPP

#include <cstddef>
#include <frg/hash.hpp>
#include <frg/hash_map.hpp>
#include <mm/mm.hpp>
#include <fs/vfs.hpp>

namespace vfs {
    class rootfs : public vfs::filesystem {
        private:
            struct storage {
                void *buf;
                ssize_t length;
            };

            pathmap<storage> file_storage;
        public:
            rootfs() : file_storage(path_hasher()) {}

            node *lookup(const pathlist& filepath, frg::string_view path, int64_t flags) override;
            
            ssize_t read(node *file, void *buf, ssize_t len, ssize_t offset) override;
            ssize_t write(node *file, void *buf, ssize_t len, ssize_t offset) override;
            ssize_t create(path name, node *parent, node *nnode, int64_t type, int64_t flags) override;
            ssize_t mkdir(const pathlist& dirpath, int64_t flags) override;
            ssize_t remove(node *dest) override;
            ssize_t lsdir(node *dir, pathlist& names) override;
    };
};

#endif