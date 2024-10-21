#include "frg/rbtree.hpp"
#include "frg/utility.hpp"
#include <mm/common.hpp>
#include <util/log/log.hpp>
#include <cstddef>
#include <cstdint>
#include <mm/mm.hpp>
#include <mm/pmm.hpp>
#include <util/lock.hpp>
#include <util/string.hpp>
#include <util/log/panic.hpp>

namespace memory::mm::allocator {
    struct header {
        size_t block_size;
        size_t pad;
    };

    struct node {
        frg::rbtree_hook hook;
        char *addr;
        size_t block_size;
    };

    struct node_comparator;
    struct node_aggregator;

    using free_tree = frg::rbtree<node, &node::hook, node_comparator>;

    struct node_comparator {
        bool operator() (node& a, node& b) {
            return a.addr < b.addr;
        };
    };

    struct node_aggregator {
        static bool aggregate(node *node) {
            size_t size = node->block_size;

            auto right = free_tree::get_right(node);
            auto left = free_tree::get_left(node);

            if (left->addr + left->block_size == node->addr) {
                node->addr = left->addr;
                node->block_size += left->block_size;

                size += left->block_size;
            }

            if (right->addr == node->addr + node->block_size) {
                node->block_size += right->block_size;

                size += right->block_size;
            }

            if (node->block_size == size) {
                return false;
            }

            return true;
        };

        static bool check_invariant(free_tree &tree, node *node) {
            return true;
        }
    };

    size_t block_size = 16;
    size_t block_align = sizeof(header);

    util::lock mm_lock{};

    free_tree blocks{};

    void init() {
        node *first = new(pmm::alloc(block_size)) node();
        first->block_size = block_size * common::page_size;
        first->addr = (char *) first;

        blocks.insert(first);
    }

    size_t calc_padding(uintptr_t ptr, uintptr_t alignment, size_t header_size) {
        uintptr_t p, a, mod, pad, space;

        p = ptr;
        a = alignment;
        mod = p & (a - 1);

        pad = 0;
        space = 0;

        if (mod != 0) {
            pad = a - mod;
        }

        space = (uintptr_t) header_size;
        if (pad < space) {
            space -= pad;

            if ((space & (a - 1)) != 0) {
                pad += a * (1 + (space / a));
            } else {
                pad += a * (space / a);
            }
        }

        return pad;
    }

    node *find_best(size_t size, size_t &pad_out) {
        size_t smallest_diff = ~(size_t) 0;

        allocator::node *node = blocks.first();
        allocator::node *best = nullptr;
        size_t pad = 0;

        while (node != nullptr) {
            pad = calc_padding((uintptr_t) node->addr, (uintptr_t) block_align, sizeof(header));
            size_t space = size + pad;
            if (node->block_size >= space && (node->block_size - space < smallest_diff)) {
                smallest_diff = node->block_size - space;
                best = node;
            }

            node = blocks.successor(node);
        }

        if (pad) pad_out = pad;
        return best;
    }

    void *malloc(size_t req_size) {
        mm_lock.irq_acquire();
        
        if (blocks.get_root() == nullptr) {
            init();
        }

        find:
            size_t pad = 0;
            allocator::node *node = nullptr;
            size_t align_pad, space, rest;
            header *hdr;

            if (req_size < sizeof(allocator::node)) {
                req_size = sizeof(allocator::node);
            }

            node = find_best(req_size, pad);
        if (node == nullptr) {
            size_t pages = frg::max(block_size, req_size / common::page_size) + 1;
            allocator::node *free_space = new(pmm::alloc(pages)) allocator::node();
            free_space->addr = (char *) free_space;
            free_space->block_size = pages * common::page_size;
            
            blocks.insert(free_space);
            goto find;
        }

        align_pad = pad - sizeof(header);
        space = req_size + pad;
        rest = node->block_size - space;
        
        if (rest > 0) {
            allocator::node *rem = new ((char *) node + space) allocator::node();
            rem->addr = (char *) rem;
            rem->block_size = rest;

            blocks.insert(rem);
        }

        blocks.remove(node);

        hdr = (header *)((char *) node + align_pad);
        hdr->block_size = space;
        hdr->pad = align_pad;

        mm_lock.irq_release();

        return (void *)((char *) hdr + sizeof(header));
    }

    void free(void *ptr) {
        mm_lock.irq_acquire();

        if (blocks.get_root() == nullptr) {
            panic("[MM] Use before init");
        }

        header *hdr;
        allocator::node *free_space;

        if (ptr == nullptr) {
            mm_lock.irq_release();
            return;
        }

        hdr = (header *)((char *) ptr - sizeof(header));
        free_space = new (hdr) allocator::node();
        free_space->addr = ((char *) free_space);
        free_space->block_size = hdr->block_size + hdr->pad;

        blocks.insert(free_space);

        mm_lock.irq_release();
    }

    void *calloc(size_t nr_items, size_t size) {
        return malloc(nr_items * size);
    }

    void *realloc(void *p, size_t size) {
        void *prev = p;
        header *hdr = (header *) ((char *) prev - sizeof(header));

        void *ptr = malloc(size);

        memcpy(ptr, prev, hdr->block_size);
        free(prev);

        return ptr;
    }
}