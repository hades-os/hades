#include "fs/devfs.hpp"
#include <frg/vector.hpp>
#include <cstdint>
#include <mm/mm.hpp>
#include <cstddef>
#include <fs/fat.hpp>
#include <fs/vfs.hpp>

static constexpr size_t FAT12 = 0x1;
static constexpr size_t FAT16 = 0x2;
static constexpr size_t FAT32 = 0x3;

static bool is_bad(uint32_t entry) {
    switch (entry) {
        case 0x0FF7:
        case 0xFFF7:
        case 0x0FFFFFF7:
            return true;
        default:
            return false;
    }
}

bool vfs::fatfs::is_eof(uint32_t entry) {
    switch (this->type) {
        case FAT12:
            if (entry >= 0x0FF8) return true;
            break;
        case FAT16:
            if (entry >= 0xFFF8) return true;
            break;
        case FAT32:
            if (entry >= 0x0FFFFFF8) return true;
            break;
    }

    return false;
}

uint32_t vfs::fatfs::rw_entry(size_t cluster, bool rw, size_t val) {
    uint32_t entry = -1;
    size_t fatOff = -1;
    size_t fatSecNum = -1;
    size_t fatEntOffset = -1;
    switch (this->type) {
        case FAT12:
            fatOff = cluster + (cluster / 2);
            break;
        case FAT16:
            fatOff = cluster * 2;
            break;
        case FAT32:
            fatOff = cluster * 4;
            break;
    }

    fatSecNum = this->superblock->rsvdSec + (fatOff / this->superblock->bytesPerSec);
    fatEntOffset = fatOff % this->superblock->bytesPerSec;

    auto device = this->source;
    uint8_t *secBuff = nullptr;
    if (this->type == FAT12) {
        secBuff = (uint8_t *) memory::mm::allocator::malloc(1024);
        devfs->read(device, secBuff, 1024, fatSecNum);
    } else {
        secBuff = (uint8_t *) memory::mm::allocator::malloc(512);
        devfs->read(device, secBuff, 512, fatSecNum);
    }

    switch (this->type) {
        case FAT12:
            entry = *((uint16_t *) &secBuff[fatEntOffset]);
            if (cluster & 0x0001) {
                entry >>= 4;
            } else {
                entry &= 0xFFF;
            }
            break;
        case FAT16:
            entry = *((uint16_t *) &secBuff[fatEntOffset]);
            break;
        case FAT32:
            entry =  *((uint32_t *) &secBuff[fatEntOffset]) & 0x0FFFFFFF;
            break;
    }

    if (rw) {
        if (is_bad(entry)) {
            return -1;
        }

        switch (this->type) {
            case FAT12:
                if (cluster & 0x0001) {
                    val <<= 4;
                    *((uint16_t *) &secBuff[fatEntOffset]) &= 0x000F;
                } else {
                    val &= 0x0FFF;
                    *((uint16_t *) &secBuff[fatEntOffset]) &= 0xF000;
                }
                *((uint16_t *) &secBuff[fatEntOffset]) |= val;
                break;
            case FAT16:
                *((uint16_t *) &secBuff[fatEntOffset]) = val;
                break;
            case FAT32:
                val = val & 0x0FFFFFFF;
                *((uint32_t *) &secBuff[fatEntOffset]) &= 0xF0000000;
                *((uint32_t *) &secBuff[fatEntOffset]) |= val;
                break; 
        }

        if (this->type == FAT12) {
            devfs->write(device, secBuff, 1024, fatSecNum);
        } else {
            devfs->write(device, secBuff, 512, fatSecNum);
        }

        return val;
    }

    return entry;
}

void *vfs::fatfs::rw_clusters(size_t begin, void *buf, bool rw, ssize_t len) {
    if (rw) {
        if (len <= 0) return nullptr;
        size_t cluster_count = len / (superblock->bytesPerSec * superblock->secPerClus);
        frg::vector<size_t, memory::mm::heap_allocator> cluster_chain{};
        for (size_t i = 0; i < cluster_count; i++) {
            cluster_chain.push_back(free_list.pop());
        }
        
        size_t buf_offset = 0;
        size_t sec_offset = 0;
        const size_t clus_size = superblock->secPerClus * superblock->bytesPerSec;
        switch(this->type) {
            case FAT12:
            case FAT16:
                sec_offset = superblock->rsvdSec + (superblock->n_fat * superblock->secPerFAT) + (superblock->n_root * 32 + superblock->bytesPerSec - 1) / superblock->bytesPerSec;
                break;
            case FAT32:
                sec_offset = superblock->rsvdSec + (superblock->n_fat * superblock->secPerFAT);
        }

        size_t clus = 0;
        for (size_t clus = cluster_chain.pop(); cluster_chain.size() != 0; clus = cluster_chain.pop(), buf_offset = buf_offset + clus_size) {
            if (devfs->write(this->source, buf + buf_offset, clus_size, sec_offset + (clus * superblock->secPerClus)) < 0) {
                memory::mm::allocator::free(buf);
                return nullptr;
            }
        }

        return buf;
    } else {
        frg::vector<size_t, memory::mm::heap_allocator> cluster_chain{};
        uint32_t entry = rw_entry(begin);
        while (!is_bad(entry) && !is_eof(entry)) {
            cluster_chain.push_back(entry);
            entry = rw_entry(entry);
        }

        if (cluster_chain.size() == 0) return nullptr;

        void *ret = memory::mm::al2locator::malloc(superblock->bytesPerSec * superblock->secPerClus * cluster_chain.size());
        size_t buf_offset = 0;
        size_t sec_offset = 0;
        const size_t clus_size = superblock->secPerClus * superblock->bytesPerSec;
        const size_t clus_offset = 2;
        switch(this->type) {
            case FAT12:
            case FAT16:
                sec_offset = superblock->rsvdSec + (superblock->n_fat * superblock->secPerFAT) + (superblock->n_root * 32 + superblock->bytesPerSec - 1) / superblock->bytesPerSec;
                break;
            case FAT32:
                sec_offset = superblock->rsvdSec + (superblock->n_fat * superblock->secPerFAT);
        }

        for (size_t clus = cluster_chain.pop(); cluster_chain.size() != 0; clus = cluster_chain.pop(), buf_offset = buf_offset + clus_size) {
            if (devfs->read(this->source, ret + buf_offset, clus_size, sec_offset + (clus * superblock->secPerClus)) < 0) {
                memory::mm::allocator::free(ret);
                return nullptr;
            }
        }

        return ret;
    }
}

void vfs::fatfs::init_fs(node *root, node *source) {
    filesystem::init_fs(root, source);

    static constexpr size_t BLKLMODE = 0x126;

    this->devfs = this->source->get_fs();
    auto device = this->source;
    this->devfs->ioctl(device, BLKLMODE, nullptr);

    this->superblock = (super *) memory::mm::allocator::malloc(512);
    devfs->read(device, this->superblock, 512, 0);

    size_t rootDirSectors = ((this->superblock->n_root * 32) + (this->superblock->bytesPerSec - 1)) / this->superblock->bytesPerSec;
    
    size_t fatSz = this->superblock->secPerFAT != 0 ? this->superblock->secPerFAT : this->superblock->ebr.nEBR.secPerFAT;
    size_t totSec = this->superblock->n_sectors != 0 ? this->superblock->n_sectors : this->superblock->large_sectors;

    size_t dataSec = totSec - (this->superblock->rsvdSec  + (this->superblock->n_fat + fatSz) + rootDirSectors);
    size_t n_clusters = dataSec / this->superblock->secPerClus;

    if (n_clusters < 4085) {
        this->type = FAT12;
    } else if (n_clusters < 65525) {
        this->type = FAT16;
    } else {
        this->type = FAT32;
    }

    for (size_t i = 2; i < n_clusters; i++) {
        size_t entry = rw_entry(i);
        if (entry == 0) free_list.push_back(i);
    }

    this->last_free = -1;
}

vfs::node *vfs::fatfs::lookup(const pathlist &filepath, vfs::path path, int64_t flags) {
    for (size_t i = 0; i < filepath.size(); i++) {
        
    }
}