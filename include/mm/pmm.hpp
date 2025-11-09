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
        struct block {
            size_t sz;
            bool is_free;
        };

        struct region {
            region *next;

            block *head;
            block *tail;

            bool has_blocks;
            size_t alignment;
        };

        struct allocation {
            region *reg;
        };

        inline util::lock pmm_lock{};
        inline region* head = nullptr;
        
        inline size_t nr_pages = 0;
        inline size_t nr_usable = 0;

        void init(stivale::boot::tags::region_map *info);

        void *alloc(size_t nr_pages);
        void *stack(size_t nr_pages);
        void *phys(size_t nr_pages);
        void free(void *address, size_t nr_pages);
    };
};

#endif