#include <cstddef>
#include <cstdint>
#include <mm/arena.hpp>
#include <mm/common.hpp>
#include <mm/mm.hpp>
#include <mm/pmm.hpp>
#include <util/string.hpp>
#include "util/lock.hpp"

frg::tuple<
    arena::arena::free_header *,
    arena::arena::free_header *,
    size_t
> arena::arena::find_best(size_t size, size_t alignment) {
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
            }

            prev = free_header;
        }

        return {prev, best, padding};
    }

    return {};
}

void arena::arena::coalesce(free_header *prev, free_header *free_block) {
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

void arena::arena::create_free_list(size_t pages) {
    auto addr = pmm::alloc(pages);
    auto block = new(addr) page_block(pages);

    block->addr = (uintptr_t) addr;
    block->page_count = pages;

    auto free_list_start = (void *) (block->addr + sizeof(page_block));
    auto free_list = new (free_list_start) free_header((pages * memory::page_size) - (sizeof(free_header) + sizeof(page_block)), block);

    block->free_list.push_front(free_list);
    block_list.push_front(block);
}

void *arena::arena::allocate(size_t size, size_t alignment) {
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

    size_t align_padding = padding - alloc_header_size;
    size_t needed_space = size + padding;

    size_t rest = chosen->size - needed_space;
    if (rest > 0) {
        free_header *rem = new ((void *) ((uintptr_t) chosen + needed_space)) free_header(rest, chosen->block);
        chosen->block->free_list.insert(chosen, rem);
    }

    chosen->block->free_list.erase(chosen);

    uintptr_t header_addr = (uintptr_t) chosen + align_padding;
    uintptr_t data_addr = header_addr + alloc_header_size;

    allocation_header *header = new ((void *) header_addr) allocation_header();
    header->size = needed_space;
    header->padding = align_padding;
    header->block = chosen->block;

    return (void *) data_addr;
}

void arena::arena::deallocate(void *ptr) {
    util::lock_guard guard{lock};

    uintptr_t current_addr = (uintptr_t) ptr;
    uintptr_t header_addr = current_addr - sizeof(allocation_header);

    allocation_header *header = (allocation_header *) header_addr;
    free_header *freed = new ((void *) header_addr) free_header(header->size + header->padding, header->block);

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

arena::arena::arena(): block_list() {
    create_free_list(memory::initialArenaSize);
}

arena::arena::~arena() {
    auto it = block_list.begin();
    while (it != block_list.end()) {
        auto page = *it;
        it = block_list.next(page);

        pmm::free((void *) page->addr);
    }
}

void *arena::allocator::allocate(size_t size, size_t alignment) const {
    return arena.allocate(size, alignment);
}

void *arena::allocator::reallocate(void *ptr, size_t size) const {
    if (!ptr)
        return arena.allocate(size, 8);

    uintptr_t current_addr = (uintptr_t) ptr;
    uintptr_t header_addr = current_addr - sizeof(arena::allocation_header);
    arena::allocation_header *header = (arena::allocation_header *) header_addr;

    void *res = arena.allocate(size, 8);
    memcpy(res, ptr, header->size);
    arena.deallocate(ptr);

    return res;
}

void arena::allocator::deallocate(void *ptr) const {
    if (!ptr)
        return;

    arena.deallocate(ptr);
}