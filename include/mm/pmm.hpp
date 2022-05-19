#ifndef PMM_HPP
#define PMM_HPP

#include <cstddef>
#include <cstdint>
#include <util/stivale.hpp>
#include <util/lock.hpp>
#include <mm/common.hpp>

#define pow2(x) (1 << (x))

namespace memory {
    namespace pmm {
        namespace {
            inline util::lock lock{};
        };

        inline size_t nr_pages;
        inline size_t nr_reserved;
        inline size_t nr_usable;

        inline void *map;
        inline size_t map_len;

        void init(stivale::boot::tags::region_map *info);

        void *alloc(size_t nr_pages);
        void *stack(size_t nr_pages);
        void *phys(size_t nr_pages);
        void free(void *address, size_t nr_pages);
    };
};

#endif