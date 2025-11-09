#include <cstddef>
#include <cstdint>
#include <mm/arena.hpp>
#include <mm/common.hpp>
#include <mm/mm.hpp>
#include <mm/pmm.hpp>
#include <util/string.hpp>
#include "util/lock.hpp"

frg::tuple<
    arena::free_list_arena::free_header *,
    arena::free_list_arena::free_header *,
    size_t
> arena::free_list_arena::find_best(size_t size, size_t alignment) {
    for (auto page_it = pages_list.begin(); page_it != pages_list.end(); page_it++) {
        auto page = *page_it;
        size_t smallestDiff = SIZE_MAX;

        auto it = page->free_list.begin();
        free_header *best, *prev = nullptr;
        size_t padding = 0;
        while (it != page->free_list.end()) {
            auto free_header=  *it;
            padding = mm::calc_padding((uintptr_t) free_header, alignment, sizeof(allocation_header));
            size_t needed_space = size + padding;
            if (free_header->size >= needed_space && (free_header->size - needed_space < smallestDiff)) {
                smallestDiff = free_header->size - needed_space;
                best = free_header;
            }

            prev = free_header;
            it = page->free_list.next(prev);
        }

        return {prev, best, padding};
    }

    return {};
}

void arena::free_list_arena::coalesce(free_header *prev, free_header *free_block) {
    auto page_list = free_block->list;
    auto free_list = page_list->free_list;

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

void arena::free_list_arena::create_free_list(size_t pages) {
    auto addr = pmm::alloc(pages);
    auto page = new(addr) page_list(pages);

    page->addr = (uintptr_t) addr;
    page->pages = pages;

    auto free_list_start = (void *) (page->addr + sizeof(page_list));
    auto free_list = new (free_list_start) free_header((pages * memory::page_size) - (sizeof(free_header) + sizeof(page_list)), page);

    page->free_list.push_front(free_list);
    pages_list.push_front(page);
}

void *arena::free_list_arena::allocate(size_t size, size_t alignment) {
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
        free_header *rem = new ((void *) ((uintptr_t) chosen + needed_space)) free_header(rest, chosen->list);
        chosen->list->free_list.insert(chosen, rem);
    }

    chosen->list->free_list.erase(chosen);

    uintptr_t header_addr = (uintptr_t) chosen + align_padding;
    uintptr_t data_addr = header_addr + alloc_header_size;

    allocation_header *header = new ((void *) header_addr) allocation_header();
    header->size = needed_space;
    header->padding = align_padding;
    header->list = chosen->list;

    return (void *) data_addr;
}

void arena::free_list_arena::deallocate(void *ptr) {
    util::lock_guard guard{lock};

    uintptr_t current_addr = (uintptr_t) ptr;
    uintptr_t header_addr = current_addr - sizeof(allocation_header);

    allocation_header *header = (allocation_header *) header_addr;
    free_header *freed = new ((void *) header_addr) free_header(header->size + header->padding, header->list);

    auto page = freed->list;
    auto free_list = page->free_list;

    auto it = free_list.begin();
    free_header *prev = nullptr;
    while (it != page->free_list.end()) {
        if (ptr < *it) {
            free_list.insert(prev, freed);
            break;
        }

        prev = *it;
        it = page->free_list.next(prev);
    }

    coalesce(prev, freed);
}

arena::free_list_arena::free_list_arena(): pages_list() {
    create_free_list(memory::initialArenaSize);
}

arena::free_list_arena::~free_list_arena() {
    auto it = pages_list.begin();
    while (it != pages_list.end()) {
        auto page = *it;
        it = pages_list.next(page);

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
    uintptr_t header_addr = current_addr - sizeof(free_list_arena::allocation_header);
    free_list_arena::allocation_header *header = (free_list_arena::allocation_header *) header_addr;

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