#ifndef ARENA_HPP
#define ARENA_HPP

#include <cstddef>
#include <cstdint>
#include <mm/mm.hpp>
#include <frg/intrusive.hpp>
#include <frg/list.hpp>
#include <frg/tuple.hpp>
#include <utility>
#include "util/lock.hpp"

namespace arena {
    struct allocator;
    struct free_list_arena {
        private:
            friend struct allocator;

            struct page_list;
            struct free_header {
                size_t size;
                page_list *list;
                frg::default_list_hook<free_header> hook;

                free_header(size_t size, page_list *list): size(size), list(list), hook() {}
            };

            struct page_list {
                size_t pages;
                uintptr_t addr;

                frg::default_list_hook<page_list> hook;
                frg::intrusive_list<
                    free_header,
                    frg::locate_member<
                        free_header,
                        frg::default_list_hook<free_header>,
                        &free_header::hook
                    >
                > free_list;

                page_list(size_t pages): pages(pages), hook(), free_list() {}
            };

            struct allocation_header {
                size_t size;
                uint8_t padding;
                page_list *list;
            };

            frg::intrusive_list<
            page_list,
                frg::locate_member<
                page_list,
                    frg::default_list_hook<page_list>,
                    &page_list::hook
                >
            > pages_list;

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
            free_list_arena();

            free_list_arena(const free_list_arena& other):
                pages_list(other.pages_list), lock() {}
            ~free_list_arena();

            void *allocate(size_t size, size_t alignment = 0);
            void deallocate(void *ptr);
    };

    struct allocator: mm::allocator {
        private:
            mutable free_list_arena arena;
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