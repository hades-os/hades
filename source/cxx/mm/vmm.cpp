#include <cstddef>
#include <cstdint>
#include <frg/allocation.hpp>
#include <mm/common.hpp>
#include <mm/mm.hpp>
#include <mm/pmm.hpp>
#include <mm/vmm.hpp>
#include <util/log/log.hpp>
#include <util/io.hpp>

namespace memory {
    namespace vmm {
        // TODO: aarch64 support, powerpc support
        namespace x86 {
            void *_virt(uint64_t phys) {
                return (void *) (phys + memory::common::virtualBase);
            }

            void *_virt(void *phys) {
                return (void *) memory::common::offsetVirtual(phys);
            }

            void *_phys(void *virt) {
                if (((uint64_t) virt) >= memory::common::kernelBase) {
                    return (void *) (((uint64_t) virt) - memory::common::kernelBase);
                }

                if (((uint64_t) virt) >= memory::common::virtualBase) {
                    return memory::common::removeVirtual(virt);
                }

                return virt;
            }

            void *_phys(uint64_t virt) {
                if (virt >= memory::common::kernelBase) {
                    return (void *) (virt - memory::common::kernelBase);
                }

                if (virt >= memory::common::virtualBase) {
                    return (void *) (virt - memory::common::virtualBase);
                }

                return (void *) virt;
            }

            void _ref(void *ptr) {
                uint64_t idx = ((uint64_t) _phys(ptr)) / memory::common::page_size;
                if (memory::vmm::common::refs[idx] == -1) {
                    memory::vmm::common::refs[idx] = 0;
                }

                memory::vmm::common::refs[idx]++;
            }

            void _ref(void *ptr, uint64_t len) {
                for (size_t i = 0; i < len; i++) {
                    _ref((char *) ptr + (memory::common::page_size * i));
                }
            }

            void _free(void *ptr) {
                uint64_t idx = ((uint64_t) _phys(ptr)) / memory::common::page_size;
                if (memory::vmm::common::refs[idx] == -1) {
                    return;
                }

                if (!(--memory::vmm::common::refs[idx])) {
                    memory::vmm::common::refs[idx] = -1;
                    memory::pmm::free(ptr, 1);
                }
            }

            void _free(void *ptr, uint64_t len) {
                for (size_t i = 0; i < len; i++) {
                    _free((char *) ptr + (memory::common::page_size * i));
                }
            }

            void *_map(void *phys, void *virt, uint64_t flags, void *ptr) {
                auto ctx = (memory::vmm::vmm_ctx *) ptr;
                uint64_t p4idx = ((uint64_t) virt >> 39) & 0x1FF;
                uint64_t p3idx = ((uint64_t) virt >> 30) & 0x1FF;
                uint64_t p2idx = ((uint64_t) virt >> 21) & 0x1FF;
                uint64_t p1idx = ((uint64_t) virt >> 12) & 0x1FF;

                uint64_t *p4 = ctx->map;
                uint64_t* p3 = nullptr;
                uint64_t* p2 = nullptr;
                uint64_t* p1 = nullptr;

                if (p4[p4idx] & VMM_PRESENT) {
                    p3 = (uint64_t *) _virt(p4[p4idx] & VMM_ADDR_MASK);
                } else {
                    p3 = (uint64_t *) memory::pmm::phys(1);
                    p4[p4idx] = (uint64_t) p3 | VMM_PRESENT | flags;
                    p3 = (uint64_t *) _virt(p3);
                }

                if (p3[p3idx] & VMM_PRESENT) {
                    p2 = (uint64_t *) _virt(p3[p3idx] & VMM_ADDR_MASK);
                } else {
                    p2 = (uint64_t *) memory::pmm::phys(1);
                    p3[p3idx] = (uint64_t) p2 | VMM_PRESENT | flags;
                    p2 = (uint64_t *) _virt(p2);
                }

                if (p2[p2idx] & VMM_PRESENT) {
                    p1 = (uint64_t *) _virt(p2[p2idx] & VMM_ADDR_MASK);
                } else {
                    p1 = (uint64_t *) memory::pmm::phys(1);
                    p2[p2idx] = (uint64_t) p1 | VMM_PRESENT | flags;
                    p1 = (uint64_t *) _virt(p1);
                }

                p1[p1idx] = ((uint64_t) phys) | flags;

                return virt;
            }

            void *_map2(void *phys, void *virt, uint64_t flags, void *ptr) {
                auto ctx = (memory::vmm::vmm_ctx *) ptr;
                uint64_t p4idx = ((uint64_t) virt >> 39) & 0x1FF;
                uint64_t p3idx = ((uint64_t) virt >> 30) & 0x1FF;
                uint64_t p2idx = ((uint64_t) virt >> 21) & 0x1FF;

                uint64_t *p4 = ctx->map;
                uint64_t* p3 = nullptr;
                uint64_t* p2 = nullptr;

                if (p4[p4idx] & VMM_PRESENT) {
                    p3 = (uint64_t *) _virt(p4[p4idx] & VMM_ADDR_MASK);
                } else {
                    p3 = (uint64_t *) memory::pmm::phys(1);
                    p4[p4idx] = (uint64_t) p3 | VMM_PRESENT | (flags & (~VMM_LARGE));
                    p3 = (uint64_t *) _virt(p3);
                }

                if (p3[p3idx] & VMM_PRESENT) {
                    p2 = (uint64_t *) _virt(p3[p3idx] & VMM_ADDR_MASK);
                } else {
                    p2 = (uint64_t *) memory::pmm::phys(1);
                    p3[p3idx] = (uint64_t) p2 | VMM_PRESENT | (flags & (~VMM_LARGE));
                    p2 = (uint64_t *) _virt(p2);
                }

                p2[p2idx] = ((uint64_t) phys) | flags | VMM_LARGE;

                return virt;
            }

            void *_unmap(void *virt, void *ptr) {
                auto ctx = (memory::vmm::vmm_ctx *) ptr;
                uint64_t p4idx = ((uint64_t) virt >> 39) & 0x1FF;
                uint64_t p3idx = ((uint64_t) virt >> 30) & 0x1FF;
                uint64_t p2idx = ((uint64_t) virt >> 21) & 0x1FF;
                uint64_t p1idx = ((uint64_t) virt >> 12) & 0x1FF;

                uint64_t *p4 = ctx->map;
                uint64_t* p3 = nullptr;
                uint64_t* p2 = nullptr;
                uint64_t* p1 = nullptr;

                if (p4[p4idx] & VMM_PRESENT) {
                    p3 = (uint64_t *) _virt(p4[p4idx] & VMM_ADDR_MASK);
                } else {
                    return nullptr;
                }

                if (p3[p3idx] & VMM_PRESENT) {
                    p2 = (uint64_t *) _virt(p3[p3idx] & VMM_ADDR_MASK);
                } else {
                    return nullptr;
                }

                if (p2[p2idx] & VMM_PRESENT) {
                    p1 = (uint64_t *) _virt(p2[p2idx] & VMM_ADDR_MASK);
                } else {
                    return nullptr;
                }

                if (p1[p1idx] & VMM_MANAGED) {
                    _free((void *) ((uint64_t) _virt(p1[p1idx]) & VMM_ADDR_MASK));
                }

                p1[p1idx] = 0;

                return virt;
            }

            void *_unmap2(void *virt, void *ptr) {
                auto ctx = (memory::vmm::vmm_ctx *) ptr;
                uint64_t p4idx = ((uint64_t) virt >> 39) & 0x1FF;
                uint64_t p3idx = ((uint64_t) virt >> 30) & 0x1FF;
                uint64_t p2idx = ((uint64_t) virt >> 21) & 0x1FF;

                uint64_t *p4 = ctx->map;
                uint64_t* p3 = nullptr;
                uint64_t* p2 = nullptr;

                if (p4[p4idx] & VMM_PRESENT) {
                    p3 = (uint64_t *) _virt(p4[p4idx] & VMM_ADDR_MASK);
                } else {
                    return nullptr;
                }

                if (p3[p3idx] & VMM_PRESENT) {
                    p2 = (uint64_t *) _virt(p3[p3idx] & VMM_ADDR_MASK);
                } else {
                    return nullptr;
                }

                if (p2[p2idx] & VMM_MANAGED) {
                    _free(_virt(p2[p2idx] & VMM_ADDR_MASK), 512);
                }

                p2[p2idx] = 0;

                return virt;
            }

            void *_perms(void *virt, uint64_t flags, void *ptr) {
                auto ctx = (memory::vmm::vmm_ctx *) ptr;
                uint64_t p4idx = ((uint64_t) virt >> 39) & 0x1FF;
                uint64_t p3idx = ((uint64_t) virt >> 30) & 0x1FF;
                uint64_t p2idx = ((uint64_t) virt >> 21) & 0x1FF;
                uint64_t p1idx = ((uint64_t) virt >> 12) & 0x1FF;

                uint64_t *p4 = ctx->map;
                uint64_t* p3 = nullptr;
                uint64_t* p2 = nullptr;
                uint64_t* p1 = nullptr;

                if (p4[p4idx] & VMM_PRESENT) {
                    p3 = (uint64_t *) _virt(p4[p4idx] & VMM_ADDR_MASK);
                } else {
                    return nullptr;
                }

                if (p3[p3idx] & VMM_PRESENT) {
                    p2 = (uint64_t *) _virt(p3[p3idx] & VMM_ADDR_MASK);
                } else {
                    return nullptr;
                }

                if (p2[p2idx] & VMM_PRESENT) {
                    p1 = (uint64_t *) _virt(p2[p2idx] & VMM_ADDR_MASK);
                } else {
                    return nullptr;
                }

                uint64_t phys = (uint64_t) p1[p1idx] & VMM_ADDR_MASK;
                p1[p1idx] = phys | flags;

                return virt;
            }

            void *_perms2(void *virt, uint64_t flags, void *ptr) {
                auto ctx = (memory::vmm::vmm_ctx *) ptr;
                uint64_t p4idx = ((uint64_t) virt >> 39) & 0x1FF;
                uint64_t p3idx = ((uint64_t) virt >> 30) & 0x1FF;
                uint64_t p2idx = ((uint64_t) virt >> 21) & 0x1FF;

                uint64_t *p4 = ctx->map;
                uint64_t* p3 = nullptr;
                uint64_t* p2 = nullptr;

                if (p4[p4idx] & VMM_PRESENT) {
                    p3 = (uint64_t *) _virt(p4[p4idx] & VMM_ADDR_MASK);
                } else {
                    return nullptr;
                }

                if (p3[p3idx] & VMM_PRESENT) {
                    p2 = (uint64_t *) _virt(p3[p3idx] & VMM_ADDR_MASK);
                } else {
                    return nullptr;
                }

                uint64_t phys = (uint64_t) p2[p2idx] & VMM_ADDR_MASK;
                p2[p2idx] = phys | flags | VMM_LARGE;

                return virt;
            }

            void *_remap(void *virt, void *phys, uint64_t flags, void *ptr) {
                auto ctx = (memory::vmm::vmm_ctx *) ptr;
                uint64_t p4idx = ((uint64_t) virt >> 39) & 0x1FF;
                uint64_t p3idx = ((uint64_t) virt >> 30) & 0x1FF;
                uint64_t p2idx = ((uint64_t) virt >> 21) & 0x1FF;
                uint64_t p1idx = ((uint64_t) virt >> 12) & 0x1FF;

                uint64_t *p4 = ctx->map;
                uint64_t* p3 = nullptr;
                uint64_t* p2 = nullptr;
                uint64_t* p1 = nullptr;

                if (p4[p4idx] & VMM_PRESENT) {
                    p3 = (uint64_t *) _virt(p4[p4idx] & VMM_ADDR_MASK);
                } else {
                    return nullptr;
                }

                if (p3[p3idx] & VMM_PRESENT) {
                    p2 = (uint64_t *) _virt(p3[p3idx] & VMM_ADDR_MASK);
                } else {
                    return nullptr;
                }

                if (p2[p2idx] & VMM_PRESENT) {
                    p1 = (uint64_t *) _virt(p2[p2idx] & VMM_ADDR_MASK);
                } else {
                    return nullptr;
                }

                if (p1[p1idx] & VMM_MANAGED) {
                    _free((void *) ((uint64_t) _virt(p1[p1idx]) & VMM_ADDR_MASK));
                }

                p1[p1idx] = (uint64_t) phys | flags;

                return virt;
            }

            void *_remap2(void *virt, void *phys, uint64_t flags, void *ptr) {
                auto ctx = (memory::vmm::vmm_ctx *) ptr;
                uint64_t p4idx = ((uint64_t) virt >> 39) & 0x1FF;
                uint64_t p3idx = ((uint64_t) virt >> 30) & 0x1FF;
                uint64_t p2idx = ((uint64_t) virt >> 21) & 0x1FF;

                uint64_t *p4 = ctx->map;
                uint64_t* p3 = nullptr;
                uint64_t* p2 = nullptr;

                if (p4[p4idx] & VMM_PRESENT) {
                    p3 = (uint64_t *) _virt(p4[p4idx] & VMM_ADDR_MASK);
                } else {
                    return nullptr;
                }

                if (p3[p3idx] & VMM_PRESENT) {
                    p2 = (uint64_t *) _virt(p3[p3idx] & VMM_ADDR_MASK);
                } else {
                    return nullptr;
                }

                if (p2[p2idx] & VMM_MANAGED) {
                    _free(_virt(p2[p2idx] & VMM_ADDR_MASK), 512);
                }

                p2[p2idx] = (uint64_t) phys | flags | VMM_LARGE;

                return virt;
            }
        };
    };
};

// API Functions
void memory::vmm::init() {
    common::refs_len = pmm::nr_pages * sizeof(int64_t);
    common::refs = (int64_t *) kmalloc(common::refs_len);
    for (size_t i = 0; i < pmm::nr_pages; i++) {
        common::refs[i] = -1;
    }

    common::boot_ctx = frg::construct<vmm_ctx>(mm::heap);
    common::boot_ctx->map = (uint64_t *) pmm::alloc(1);

    map(0, (void *) memory::common::kernelBase, 8, VMM_PRESENT | VMM_LARGE, common::boot_ctx);
    
    if (pmm::nr_pages * memory::common::page_size < VMM_4GIB) {
        map(0, (void *) memory::common::virtualBase, VMM_4GIB / memory::common::page_size_2MB, VMM_PRESENT | VMM_LARGE | VMM_WRITE, common::boot_ctx);
    } else {
        map(0, (void *) memory::common::virtualBase, ((pmm::nr_pages * memory::common::page_size) / memory::common::page_size_2MB), VMM_PRESENT | VMM_LARGE | VMM_WRITE, common::boot_ctx);
    }

    change(common::boot_ctx);
    common::boot_ctx->create_hole((void *) 0x100000, 0x7ffffff00000);
    kmsg("[VMM] Initialized");
}

void *memory::vmm::create() {
    common::vmm_lock.acquire();
    vmm_ctx *ctx = frg::construct<vmm_ctx>(mm::heap);
    ctx->map = (uint64_t *) pmm::alloc(1);
    /**
        for (size_t i = (VMM_ENTRIES_PER_TABLE / 2); i < VMM_ENTRIES_PER_TABLE; i++) {
        ctx->map[i] = common::boot_ctx->map[i];
    }
     */
    ctx->create_hole((void *) 0x100000, 0x7ffffff00000);
    common::vmm_lock.release();
    return (void *) ctx;
}

void memory::vmm::destroy(void *ptr) {
    auto ctx = (vmm_ctx *) ptr;

    uint64_t cr3 = 0;
    asm volatile("mov %%cr3, %%rax;    \
                  mov %%rax, %0"
                  : "=r"(cr3));

    if (cr3 == (size_t) memory::common::removeVirtual(ctx->map)) {
        change(common::boot_ctx);
    }
    
    auto hole = ctx->holes.first();
    while (hole) {
        ctx->delete_mapping(hole->addr, hole->len);
        hole = ctx->holes.successor(hole);
    }
}

void memory::vmm::change(void *ptr) {
    auto ctx = (memory::vmm::vmm_ctx *) ptr;
    asm volatile("mov %0, %%cr3;    \
                  mov %%cr3, %%rax; \
                  mov %%rax, %%cr3"
                  :
                  : "r"((size_t) memory::common::removeVirtual(ctx->map)));
}

void *memory::vmm::boot() {
    return common::boot_ctx;
}

void *memory::vmm::cr3(void *ptr) {
    auto ctx = (memory::vmm::vmm_ctx *) ptr;
    return memory::common::removeVirtual(ctx->map);
}

uint64_t memory::vmm::read_cr3() {
    uint64_t ret;
    asm volatile("movq %%cr3, %0;" : "=r"(ret));
    return ret;
}

void memory::vmm::write_cr3(uint64_t map) {
    asm volatile("movq %0, %%cr3;" ::"r"(map) : "memory");
}

void *memory::vmm::map(void *virt, uint64_t len, uint64_t flags, void *ptr) {
    auto *ctx = (vmm::vmm_ctx *) ptr;

    if (!(flags & VMM_MANAGED)) {
        return nullptr;
    }

    if (virt && flags & VMM_FIXED) {
        ctx->delete_mapping(virt, len);
    } else if (!virt && flags & VMM_FIXED) {
        return nullptr;
    }

    return ctx->create_mapping(virt, len, flags);
}

void *memory::vmm::map(void *virt, uint64_t len, uint64_t flags, void *ptr, vmm::vmm_ctx::mapping::callback_obj callbacks) {
    auto *ctx = (vmm::vmm_ctx *) ptr;

    if (!(flags & VMM_MANAGED)) {
        return nullptr;
    }

    if (virt && flags & VMM_FIXED) {
        ctx->delete_mapping(virt, len);
    } else if (!virt && flags & VMM_FIXED) {
        return nullptr;
    }

    return ctx->create_mapping(virt, len, flags, callbacks);
}

void *memory::vmm::map(void *phys, void *virt, uint64_t len, uint64_t flags, void *ptr) {
    auto *ctx = (vmm::vmm_ctx *) ptr;

    if (flags & VMM_MANAGED) {
        return nullptr;
    }

    if (flags & VMM_LARGE) {
        for (size_t i = 0; i < len; i++) {
            x86::_map2((char *) phys + (memory::common::page_size_2MB * i), (char *) virt + (memory::common::page_size_2MB * i), flags, ptr);
        }
    } else {
        for (size_t i = 0; i < len; i++) {
            x86::_map((char *) phys + (memory::common::page_size * i), (char *) virt + (memory::common::page_size * i), flags, ptr);
        }
    }

    return ctx->unmanaged_mapping(virt, len, flags);
}

void *memory::vmm::unmap(void *virt, uint64_t len, void *ptr) {
   auto *ctx = (vmm::vmm_ctx *) ptr;
   return ctx->delete_mapping(virt, len);
}