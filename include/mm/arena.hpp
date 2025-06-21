#ifndef ARENA_HPP
#define ARENA_HPP

#include <cstddef>
#include <cstdint>
#include <mm/mm.hpp>
#include <frg/intrusive.hpp>
#include <frg/tuple.hpp>
#include "prs/allocator.hpp"
#include "prs/list.hpp"
#include "util/lock.hpp"

namespace arena {
    struct arena_resource;
    arena_resource *create_resource();

    struct arena_resource: public prs::memory_resource {
        private:
            struct page_block;
            struct free_header {
                size_t size;
                page_block *block;
                prs::list_hook hook;

                free_header(size_t size, page_block *block): size(size), block(block), hook() {}
            };

            struct page_block {
                size_t page_count;
                uintptr_t addr;

                prs::list_hook hook;
                prs::list<
                    free_header,
                    &free_header::hook
                > free_list;

                page_block(size_t page_count): page_count(page_count), hook(), free_list() {}
            };

            struct allocation_header {
                size_t size;
                uint8_t padding;
                page_block *block;
            };

            prs::list<
                page_block,
                &page_block::hook
            > block_list;

            // prev, found
            frg::tuple<
                free_header *,
                free_header *,
                size_t
            > find_best(size_t size, size_t alignment);
            void coalesce(free_header *prev, free_header *free_block);

            void create_free_list(size_t pages);

            util::spinlock lock;
        public:
            friend arena_resource *create_resource();

            arena_resource():
                block_list(), lock() {}
            arena_resource(const arena_resource& other):
                block_list(other.block_list), lock() {}

            ~arena_resource() override;

            void *allocate(size_t size,
                size_t align = alignof(std::max_align_t)) override;
            void deallocate(void *ptr) override;
    };
};

#endif