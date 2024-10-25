#ifndef FAT_HPP
#define FAT_HPP

#include "frg/tuple.hpp"
#include <frg/vector.hpp>
#include <cstddef>
#include <cstdint>
#include <frg/allocation.hpp>
#include <frg/hash.hpp>
#include <frg/hash_map.hpp>
#include <mm/mm.hpp>
#include <fs/vfs.hpp>
#include <fs/dev.hpp>

namespace vfs {
    class fatfs : public vfs::filesystem {
        private:
            struct [[gnu::packed]] super {
                char _[11];
                uint16_t bytesPerSec;
                uint8_t secPerClus;
                uint16_t rsvdSec;
                uint8_t n_fat;
                uint16_t n_root;
                uint16_t n_sectors;
                uint8_t media;
                uint16_t secPerFAT;
                uint16_t secPerTrack;
                uint16_t heads;
                uint32_t hidden;
                uint32_t large_sectors;

                union {
                    struct [[gnu::packed]] fat32 {
                        uint32_t secPerFAT;
                        uint16_t flags;
                        uint16_t version;
                        uint32_t rootClus;
                        uint16_t fsInfo;
                        uint16_t bBootSec;
                        char rsvd[12];
                        uint8_t drive;
                        uint8_t NTflags;
                        uint8_t sig;
                        uint32_t serial;
                        char label[11];
                        char ident[8];
                        char bcode[420];
                        uint16_t partSig;
                    };

                    struct [[gnu::packed]] fat16 {
                        uint8_t drive;
                        uint8_t NTflags;
                        uint8_t sig;
                        uint32_t serial;
                        char label[11];
                        char ident[8];
                        char bcode[448];
                        uint16_t partSig;
                    };

                    fat16 oEBR;
                    fat32 nEBR;
                } ebr;
            };

            struct [[gnu::packed]] fsInfo {
                uint32_t lSig;
                char rsvd[480];
                uint32_t mSig;
                uint32_t n_free_clus;
                uint32_t aval_clus;
                uint16_t rsvd1 : 12;
                uint32_t tSig;
            };

            struct [[gnu::packed]] fatEntry {
                char name[11];
                uint8_t attr;
                uint8_t NTrsvd;
                uint8_t mkTimeTs;
                uint16_t mkTime;
                uint16_t mkDate;
                uint16_t accessDate;
                uint16_t clus_hi;
                uint16_t modTime;
                uint16_t modDate;
                uint16_t clus_lo;
                uint32_t size;
            };

            struct [[gnu::packed]] lfn {
                uint8_t pos;
                char head[10];
                uint8_t ign;
                uint8_t type;
                uint8_t sfnChecksum;
                char middle[12];
                uint16_t zero;
                char tail[4];
            };

            super *superblock = nullptr;
            frg::hash_map<vfs::path, size_t, vfs::path_hasher, memory::mm::heap_allocator> sector_map;
            frg::vector<size_t, memory::mm::heap_allocator> free_list;
            
            uint8_t *fat;
            uint8_t type;
            vfs::devfs *devfs;

            ssize_t last_free;

            typedef frg::tuple<void *, size_t> rw_result;
            rw_result rw_clusters(size_t begin, void *buf, ssize_t offset = 0, ssize_t len = 0, bool read_all = false, bool rw = false);
            uint32_t rw_entry(size_t cluster, bool rw = false, size_t val = 0);

            bool is_eof(uint32_t entry);
        public:
            fatfs() : sector_map(vfs::path_hasher()) {}

            void init_fs(node *root, node *source) override;
            
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