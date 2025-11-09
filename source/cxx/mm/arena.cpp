#include <cstddef>
#include <cstdint>
#include <mm/arena.hpp>
#include <mm/common.hpp>
#include <mm/mm.hpp>
#include <mm/pmm.hpp>
#include <util/string.hpp>
#include "util/lock.hpp"

frg::tuple<
    arena::arena_resource::free_header *,
    arena::arena_resource::free_header *,
    size_t
> arena::arena_resource::find_best(size_t size, size_t alignment) {
    for (auto page_it = block_list.begin(); page_it != block_list.end(); page_it++) {
        auto page = *page_it;
        size_t smallestDiff = SIZE_MAX;

        free_header *best, *prev = nullptr;
        size_t padding = 0;
        for (auto it = page->free_list.begin(); it != page->free_list.end(); ++it) {
            auto free_header=  *it;
            padding = mm::calc_padding((uintptr_t) free_header, alignment, sizeof(allocation_header));
            size_t needed_space = size + padding;
            if (free_header->size >= needed_space && (free_header->size - needed_space < smallestDiff)) {
                smallestDiff = free_header->size - needed_space;
                best = free_header;

                if (smallestDiff == 0)
                    return {prev, best, padding};
            }

            prev = free_header;
        }

        return {prev, best, padding};
    }

    return {};
}

void arena::arena_resource::coalesce(free_header *prev, free_header *free_block) {
    auto block = free_block->block;
    auto& free_list = block->free_list;

    auto next = free_list.next(free_block);
    if (next != nullptr
        && (uintptr_t) free_block + free_block->size == (uintptr_t) next) {
        free_block->size += next->size;
        free_list.erase(next);
    }

    if (prev != nullptr
        && (uintptr_t) prev + prev->size == (uintptr_t) free_block) {
        free_list.erase(prev);
    }
}

void arena::arena_resource::create_free_list(size_t pages) {
    auto addr = pmm::alloc(pages);
    auto block = new(addr) page_block(pages);

    block->addr = (uintptr_t) addr;
    block->page_count = pages;

    auto free_list_start = (void *) (block->addr + sizeof(page_block));
    auto free_list = new (free_list_start) free_header((pages * memory::page_size) - (sizeof(free_header) + sizeof(page_block)), block);

    block->free_list.push_front(free_list);
    block_list.push_front(block);
}

void *arena::arena_resource::allocate(size_t size, size_t alignment) {
    util::lock_guard guard{lock};

    size_t alloc_header_size = sizeof(allocation_header);

    if (size < sizeof(allocation_header)) {
        size = sizeof(allocation_header);
    }

    if (alignment < 8) {
        alignment = 8;
    }

    pick_block:
        auto [prev, chosen, padding] = find_best(size, alignment);

    if (chosen == nullptr) {
        create_free_list(memory::initialArenaSize);
        goto pick_block;
    }

    auto& free_list = chosen->block->free_list;

    size_t align_padding = padding - alloc_header_size;
    size_t needed_space = size + padding;

    size_t rest = chosen->size - needed_space;
    if (rest > 0) {
        free_header *rem = new ((void *) ((uintptr_t) chosen + needed_space)) free_header(rest, chosen->block);
        free_list.insert(chosen, rem);
    }

    free_list.erase(chosen);

    uintptr_t header_addr = (uintptr_t) chosen + align_padding;
    uintptr_t data_addr = header_addr + alloc_header_size;

    allocation_header *header = new ((void *) header_addr) allocation_header();
    header->size = size;
    header->padding = padding;
    header->block = chosen->block;

    return (void *) data_addr;
}

void arena::arena_resource::deallocate(void *ptr) {
    util::lock_guard guard{lock};

    uintptr_t current_addr = (uintptr_t) ptr;
    uintptr_t header_addr = current_addr - sizeof(allocation_header);

    allocation_header *header = (allocation_header *) header_addr;
    size_t align_padding = header->padding - sizeof(allocation_header);

    free_header *freed = new ((void *) (header_addr - align_padding)) free_header(header->size + header->padding, header->block);

    auto block = freed->block;
    auto& free_list = block->free_list;

    free_header *prev = nullptr;
    for (auto it = free_list.begin(); it != free_list.end(); ++it) {
        if (ptr < *it) {
            free_list.insert(prev, freed);
            break;
        }

        prev = *it;
    }

    coalesce(prev, freed);
}

void *arena::arena_resource::reallocate(void *p, size_t new_bytes) {
    util::lock_guard guard{lock};

    uintptr_t current_addr = (uintptr_t) p;
    uintptr_t header_addr = current_addr - sizeof(allocation_header);

    allocation_header *header = (allocation_header *) header_addr;

    if (header->size < new_bytes) {
        void *new_p = allocate(new_bytes);
        size_t old_bytes = header->size;
        memcpy(new_p, p, old_bytes);

        deallocate(p);

        return new_p;
    } else {
        return p;
    }
}

arena::arena_resource::~arena_resource() {
    for (auto it = block_list.rbegin(); 
        it != block_list.rend(); ++it) {
        auto page = *it;
        pmm::free((void *) page->addr);
    }
}

arena::arena_resource *arena::create_resource() {
    auto base = pmm::alloc(memory::initialArenaSize);
    auto addr = (uintptr_t) base;
    auto resource = new (base) arena_resource();

    auto block = new((void *) (addr + sizeof(arena_resource))) 
        arena_resource::page_block(memory::initialArenaSize);

    block->addr = (uintptr_t) addr;
    block->page_count = memory::initialArenaSize;

    auto free_block_addr = (void *) (addr + sizeof(arena_resource::page_block) + sizeof(arena_resource));
    auto free_block_size = (memory::initialArenaSize * memory::page_size) - 
        (sizeof(arena_resource::free_header) + sizeof(arena_resource::page_block) + sizeof(arena_resource));

    auto free_block = new (free_block_addr) 
        arena_resource::free_header(free_block_size, block);

    block->free_list.push_front(free_block);
    resource->block_list.push_front(block);

    return resource;
}