#include <cstddef>
#include <cstdint>
#include <mm/common.hpp>
#include <mm/pmm.hpp>
#include <util/log/log.hpp>
#include <util/stivale.hpp>
#include <util/string.hpp>

void sbit(uint64_t idx) {
    asm volatile (
        "bts %0, (%1)"
        :
        : "r" (idx), "r" (memory::pmm::map)
        : "memory"
    );
}

void cbit(uint64_t idx) {
    asm volatile (
        "btr %0, (%1)"
        :
        : "r" (idx), "r" (memory::pmm::map)
        : "memory"
    );
}

bool rbit(uint64_t idx) {
    uint8_t ret = 0;
    asm volatile (
        "bt %1, (%2)"
        : "=@ccc" (ret)
        : "r" (idx), "r" (memory::pmm::map)
        : "memory"
    );

    return ret;
}

constexpr size_t base = memory::common::virtualBase + 0x1000000;

void memory::pmm::init(stivale::boot::tags::region_map *info) {
    map = (void *) (base);
    map_len = info->page_count() / 8;

    nr_pages = info->page_count();
    nr_reserved = (map_len / common::page_size) + 1;
    nr_usable = nr_pages - nr_reserved;

    memset(map, 0xFF, map_len);

    for (size_t i = 0; i < info->entries; i++) {
        auto region = info->regionmap[i];
        if (region.base < 0x100000)
            continue;
        if (region.type == stivale::boot::info::type::USABLE) {
            for (size_t i = (region.base / common::page_size); i < (region.base + region.length) / common::page_size; i++) {
                cbit(i);
            }
        }
    }

    for (size_t i = (0x1000000 / common::page_size); i < (0x1000000 / common::page_size) + (map_len / common::page_size); i++) {
        sbit(i);
    }

    kmsg("[PMM] Free memory: ", nr_usable * common::page_size, " bytes");
}

void *memory::pmm::alloc(size_t req_pages) {
    lock.acquire();
    size_t targ = req_pages;
    size_t i = 0;

    for (; i < nr_pages; i++) {
        if (!rbit(i++)) {
            if (!--targ) {
                goto out;
            }
        } else {
            targ = req_pages;
        }
    }

    panic("[PMM] Out of memory");

    out:;
    nr_usable = nr_usable - req_pages;
    size_t j = i - req_pages;
    void *address = (void *) ((j * common::page_size) + common::virtualBase);
    memset(address, 0, req_pages * common::page_size);
    for (; j < i; j++) {
        sbit(j);
    }

    lock.release();
    return address;
}

void *memory::pmm::stack(size_t req_pages) {
    return alloc(req_pages) + (req_pages * common::page_size);
}

void *memory::pmm::phys(size_t req_pages) {
    return (void *) (((uint64_t) alloc(req_pages)) - common::virtualBase);
}

void memory::pmm::free(void *address, size_t req_pages) {
    for (size_t i = ((size_t) ((uint64_t) address - common::virtualBase)) / common::page_size; i < ((size_t) ((uint64_t) address - common::virtualBase)) / common::page_size + req_pages; i++)
        cbit(i);
}