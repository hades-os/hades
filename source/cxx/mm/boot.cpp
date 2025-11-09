#include <cstddef>
#include <mm/boot.hpp>
#include <util/lock.hpp>
#include <util/log/panic.hpp>

static util::spinlock lock{};
extern "C" char __boot_heap_start[];
extern "C" char __boot_heap_end[];

static char *boot_heap = __boot_heap_start;
static char *boot_heap_end = __boot_heap_end;
static char *boot_heap_current = __boot_heap_start;
constexpr size_t boot_heap_max = 32 * memory::page_size;

void *boot::allocator::allocate(size_t size, size_t _) const {
    util::lock_guard guard{lock};
    if (boot_heap_current + size > boot_heap_end)
        panic("Unable to allocate memory from boot bump allocator");

    void *addr = boot_heap_current;
    boot_heap_current += size;

    return addr;
}

void *boot::allocator::reallocate(void *ptr, size_t size) const {
    return nullptr;
}

void boot::allocator::deallocate(void *ptr) const {
    return;
}

boot::allocator mm::boot() {
    return boot::allocator();
}