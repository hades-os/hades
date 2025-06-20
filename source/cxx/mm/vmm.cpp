#include <arch/vmm.hpp>
#include <arch/x86/types.hpp>
#include <cstddef>
#include <cstdint>
#include <prs/construct.hpp>
#include <mm/common.hpp>
#include <mm/mm.hpp>
#include <mm/pmm.hpp>
#include <mm/vmm.hpp>
#include <util/log/log.hpp>
#include <util/io.hpp>
#include "mm/slab.hpp"

int64_t *refs = nullptr;
uint64_t refs_len = 0;

vmm::vmm_ctx *vmm::boot = nullptr;
util::spinlock vmm_lock{};

static log::subsystem logger = log::make_subsystem("VM");
// API Functions
void vmm::init() {
    auto allocator = prs::allocator{slab::create_resource()};

    refs_len = pmm::nr_pages * sizeof(int64_t);
    refs = (int64_t *) allocator.allocate(refs_len);
    for (size_t i = 0; i < pmm::nr_pages; i++) {
        refs[i] = -1;
    }

    boot = prs::construct<vmm_ctx>(allocator);
    boot->page_map = new_pagemap();
    boot->setup_hole();

    for (size_t i = 0; i < 8; i++) {
        void *phys = (void *) (i * memory::page_large);
        void *addr = (void *) (memory::x86::kernelBase + (i * memory::page_large));
        map_single_2m(addr, phys, page_flags::PRESENT, boot->page_map);
    }

    if (pmm::nr_pages * memory::page_size < limit_4g) {
        for (size_t i = 0; i < limit_4g / memory::page_large; i++) {
            void *phys = (void *) (i * memory::page_large);
            void *addr = (void *) (memory::x86::virtualBase + (i * memory::page_large));
            map_single_2m(addr, phys, page_flags::PRESENT | page_flags::WRITE | page_flags::NX, boot->page_map);
        }
    } else {
        for (size_t i = 0; i < ((pmm::nr_pages) * memory::page_size) / memory::page_large; i++) {
            void *phys = (void *) (i * memory::page_large);
            void *addr = (void *) (memory::x86::virtualBase + (i * memory::page_large));
            map_single_2m(addr, phys, page_flags::PRESENT | page_flags::WRITE | page_flags::NX, boot->page_map);
        }
    }

    boot->swap_in();
    kmsg(logger, "Initialized");
}

vmm::vmm_ctx *vmm::create() {
    auto new_ctx = prs::construct<vmm_ctx>(prs::allocator{slab::create_resource()});

    new_ctx->page_map = new_pagemap();
    new_ctx->setup_hole();
    copy_boot_map(new_ctx->page_map);

    return new_ctx;
}

void vmm::destroy(vmm_ctx *ctx) {
    boot->swap_in();
    ctx->~vmm_ctx();
 
    prs::destruct(prs::allocator{slab::create_resource()}, ctx);
}