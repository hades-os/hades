#ifndef ARENA_HPP
#define ARENA_HPP

#include <cstddef>
#include <cstdint>
#include <mm/mm.hpp>
#include <frg/intrusive.hpp>
#include <frg/list.hpp>
#include <frg/tuple.hpp>
#include <utility>
#include "prs/list.hpp"
#include "util/lock.hpp"

namespace arena {
    struct allocator;
    struct arena {
        private:
            friend struct allocator;

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
            arena();

            arena(const arena& other):
            block_list(other.block_list), lock() {}
            ~arena();

            void *allocate(size_t size, size_t alignment = 0);
            void deallocate(void *ptr);
    };

    struct allocator: mm::allocator {
        private:
            mutable arena arena;
        public:
            void *allocate(size_t size, size_t alignment = 8) const override;
            void *reallocate(void *ptr, size_t size) const override;

            void deallocate(void *ptr) const override;
            using mm::allocator::deallocate;
            using mm::allocator::free;

            allocator(): mm::allocator(), arena() {}
            allocator(const allocator& other):
                mm::allocator(),
                arena(other.arena) {}
    };
};

#endif