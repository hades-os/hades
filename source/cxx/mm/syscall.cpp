#include "mm/common.hpp"
#include "sys/irq.hpp"
#include "sys/smp.hpp"
#include <cstddef>

constexpr size_t MAP_FAILED = size_t(-1);
constexpr size_t MAP_PRIVATE = 0x1;
constexpr size_t MAP_SHARED = 0x2;
constexpr size_t MAP_FIXED = 0x4;
constexpr size_t MAP_ANONYMOUS = 0x8;
constexpr size_t MAP_MIN_ADDR = 0x80000000ull;

void syscall_mmap(irq::regs *r) {
    auto process = smp::get_process();
    auto ctx = process->mem_ctx;

    void *addr = (void *) r->rdi;
    size_t len = r->rsi;
    int prot = r->rdx;
    int flags = r->r10;
    int fd = r->r8;
    size_t offset = r->r9;
    size_t pages = ((len / memory::common::page_size) + 1) * memory::common::page_size;

    ctx->lock.irq_acquire();
    if (pages == 0 || pages % memory::common::page_size != 0) {
        smp::set_errno(EINVAL);
        ctx->lock.irq_release();
        r->rax = -1;
        return;
    }

    if (((uint64_t) addr >= 0x7ffffff00000 || (uint64_t) addr <= MAP_MIN_ADDR) ||
        ((uint64_t) addr + pages) >= 0x7ffffff00000 || ((uint64_t) addr + len) <= MAP_MIN_ADDR) {
        smp::set_errno(EINVAL);
        r->rax = -1;
        return;
    }

    if (!(flags & MAP_ANONYMOUS)) {
        if (flags & MAP_SHARED) {
            // shared
        } else if (flags & MAP_PRIVATE) {
            // private file
        } else {
            smp::set_errno(EINVAL);
            ctx->lock.irq_release();
            r->rax = MAP_FAILED;
            return;
        }
    }

    auto base = memory::vmm::map(addr, pages, VMM_USER | VMM_MANAGED | prot, ctx);
    r->rax = (uint64_t) base;
    ctx->lock.irq_release();
}

void syscall_munmap(irq::regs *r) {
    auto process = smp::get_process();
    auto ctx = process->mem_ctx;

    void *addr = (void *) r->rdi;
    size_t len = r->rsi;
    size_t pages = ((len / memory::common::page_size) + 1) * memory::common::page_size;

    if (pages == 0 || pages % memory::common::page_size != 0) {
        smp::set_errno(EINVAL);
        ctx->lock.irq_release();
        r->rax = -1;
        return;
    }

    if (((uint64_t) addr >= 0x7ffffff00000 || (uint64_t) addr <= MAP_MIN_ADDR) ||
        ((uint64_t) addr + pages) >= 0x7ffffff00000 || ((uint64_t) addr + len) <= MAP_MIN_ADDR) {
        smp::set_errno(EINVAL);
        r->rax = -1;
        return;
    }

    ctx->lock.irq_acquire();

    auto res = memory::vmm::unmap(addr, pages, ctx);
    if (res == nullptr) {
        smp::set_errno(EINVAL);
        r->rax = -1;
        return;
    }

    r->rax = 0;
}