#include "mm/common.hpp"
#include <cstddef>
#include <cstdint>
#include <frg/allocation.hpp>
#include <mm/mm.hpp>
#include <mm/pmm.hpp>
#include <mm/vmm.hpp>
#include <util/log/log.hpp>
#include <util/log/panic.hpp>

bool memory::vmm::vmm_ctx::hole_aggregator::aggregate(hole *node) {
    size_t size = node->len;

    if (hole_tree::get_left(node) && hole_tree::get_left(node)->largest_hole > size) {
        size = hole_tree::get_left(node)->largest_hole;
    }

    if (hole_tree::get_right(node) && hole_tree::get_right(node)->largest_hole > size) {
        size = hole_tree::get_right(node)->largest_hole;
    }

    if (node->largest_hole == size) {
        return false;
    }

    node->largest_hole = size;
    return true;
};

bool memory::vmm::vmm_ctx::hole_aggregator::check_invariant(hole_tree &tree, hole *node) {
    size_t size = node->len;

    hole *pred = tree.predecessor(node);
    hole *sucs = tree.successor(node);

    if (hole_tree::get_left(node) && hole_tree::get_left(node)->largest_hole > size) {
        size = hole_tree::get_left(node)->largest_hole;
    }

    if (hole_tree::get_right(node) && hole_tree::get_right(node)->largest_hole > size) {
        size = hole_tree::get_left(node)->largest_hole;
    }

    if (node->largest_hole != size) {
        panic("[VMM] Hole state violated with address ", node->addr, "pagemap ", node->map);
        return false;
    }

    if (pred && node->addr < (char *) pred->addr + pred->len) {
        panic("[VMM] Hole state violated with address ", node->addr, "pagemap ", node->map);
        return false;
    }

    if (sucs && sucs->addr < (char *) node->addr + node->len) {
        panic("[VMM] Hole state violated with address ", node->addr, "pagemap ", node->map);
        return false;
    }

    return true;
}

void *memory::vmm::vmm_ctx::create_hole(void *addr, uint64_t len) {
    hole *current = this->holes.get_root();
    if (!current) {
        this->holes.insert(frg::construct<hole>(memory::mm::heap, addr, len, (void *) this));
        return addr;
    }

    if (!addr) {
        while (true) {
            if (this->holes.get_left(current) && this->holes.get_left(current)->largest_hole >= len) {
                current = this->holes.get_left(current);
                continue;
            }

            if (current->len >= len) {
                auto addr = current->addr;
                this->split_hole(current, 0, len);
                return addr;
            }

            current = this->holes.get_right(current);
        }
    } else {
        while (true) {
            if (!current) {
                kmsg("Out of virtual memory");
                return nullptr;
            }

            if (addr < current->addr) {
                current = this->holes.get_left(current);
            } else if (addr >= (char *) current->addr + current->len) {
                current = this->holes.get_right(current);
            } else {
                break;
            }
        }

        if ((char *) addr - (char *) current->addr + len > current->len) {
            kmsg("Out of virtual memory");
            return nullptr;
        }

        this->split_hole(current, (uint64_t) addr - (uint64_t) current->addr, len);
        return addr;
    }

    return nullptr;
}

uint8_t memory::vmm::vmm_ctx::delete_hole(void *addr, uint64_t len) {
    hole *current = this->holes.get_root();

    hole *pre = nullptr;
    hole *succ = nullptr;

    while (true) {
        if (addr < current->addr) {
            if (this->holes.get_left(current)) {
                current = this->holes.get_left(current);
            } else {
                pre = this->holes.predecessor(current);
                succ = current;
                break;
            }
        } else {
            if (this->holes.get_right(current)) {
                current = this->holes.get_right(current);
            } else {
                pre = current;
                succ = this->holes.successor(current);
                break;
            }
        }
    }

    if (pre && (char *) pre->addr + pre->len == addr && succ && (char *) addr + len == succ->addr) {
        hole *cur = frg::construct<hole>(memory::mm::heap, pre->addr, pre->len + len + succ->len, (void *) this);

        this->holes.remove(pre);
        this->holes.remove(succ);
        this->holes.insert(cur);

        frg::destruct(memory::mm::heap, pre);
        frg::destruct(memory::mm::heap, succ);
    } else if (pre && (char *) pre->addr + pre->len == addr) {
        hole *cur = frg::construct<hole>(memory::mm::heap, pre->addr, pre->len + len, (void *) this);

        this->holes.remove(pre);
        this->holes.insert(cur);

        frg::destruct(memory::mm::heap, pre);
    } else if (succ && (char *) addr + len == succ->addr) {
        hole *cur = frg::construct<hole>(memory::mm::heap, addr, succ->len + len, (void *) this);

        this->holes.remove(succ);
        this->holes.insert(cur);

        frg::destruct(memory::mm::heap, succ);
    } else {
        hole *cur = frg::construct<hole>(memory::mm::heap, addr, len, (void *) this);

        this->holes.insert(cur);
    }
    return 0;
}

void memory::vmm::vmm_ctx::split_hole(hole *node, uint64_t offset, size_t len) {
    this->holes.remove(node);

    if (offset) {
        hole *pred = frg::construct<hole>(memory::mm::heap, node->addr, offset, (void *) this);
        this->holes.insert(pred);
    }

    if ((offset + len) < node->len) {
        hole *sucs = frg::construct<hole>(memory::mm::heap, (char *) node->addr + offset + len, node->len - (offset + len), (void *) this);
        this->holes.insert(sucs);
    }

    frg::destruct(memory::mm::heap, node);
}

uint8_t memory::vmm::vmm_ctx::mapped(void *addr, uint64_t len) {
    mapping *current = this->mappings.get_root();
    if (!current) {
        return 0;
    }

    if (addr < current->addr) {
        while (current) {
            if (addr >= current->addr && addr <= (char *) current->addr + current->len) {
                return 1;
            }

            current = this->mappings.predecessor(current);
        }
    } else {
        while (current) {
            if (addr >= current->addr && ((char *) addr + len) <= ((char *) current->addr + current->len)) {
                return 1;
            }

            current = this->mappings.successor(current);
        }
    }

    return 0;
}

void *memory::vmm::vmm_ctx::create_mapping(void *addr, uint64_t len, uint64_t flags) {
    if (this->mapped(addr, len)) {
        return nullptr;
    }

    void *dst = this->create_hole(addr, len);

    bool fill_phys = (flags & VMM_FIXED) || (flags & VMM_WRITE && flags & VMM_PRESENT);
    if (flags & VMM_LARGE) {
        for (size_t i = 0; i <= memory::common::page_count(len) / 512; i++) {
            memory::vmm::x86::_map2(fill_phys ? pmm::phys(512) : nullptr, (char *) dst + (memory::common::page_size_2MB * i), flags, (void *) this);
        }
    } else {
        for (size_t i = 0; i <= memory::common::page_count(len); i++) {
            memory::vmm::x86::_map(fill_phys ? pmm::phys(1) : nullptr, (char *) dst + (memory::common::page_size * i), flags, (void *) this);
        }
    }

    mapping *node = frg::construct<mapping>(memory::mm::heap, dst, len, (void *) this, flags & VMM_LARGE);
    if (fill_phys) node->free_pages = true;
    node->perms = flags;

    this->mappings.insert(node);
    return dst;
}

void *memory::vmm::vmm_ctx::create_mapping(void *addr, uint64_t len, uint64_t flags, mapping::callback_obj callbacks) {
    if (this->mapped(addr, len)) {
        return nullptr;
    }

    void *dst = this->create_hole(addr, len);
    if (flags & VMM_LARGE) {
        for (size_t i = 0; i <= memory::common::page_count(len) / 512; i++) {
            memory::vmm::x86::_map2(nullptr, (char *) dst + (memory::common::page_size_2MB * i), flags, (void *) this);
        }
    } else {
        for (size_t i = 0; i <= memory::common::page_count(len); i++) {
            memory::vmm::x86::_map(nullptr, (char *) dst + (memory::common::page_size * i), flags, (void *) this);
        }
    }

    mapping *node = frg::construct<mapping>(memory::mm::heap, dst, len, (void *) this, flags & VMM_LARGE, callbacks);
    node->perms = flags;
    this->mappings.insert(node);
    return dst;
}

void memory::vmm::vmm_ctx::copy_mappings(memory::vmm::vmm_ctx *other) {
    mapping *current = other->mappings.first();
    while (current) {
        mapping *node = frg::construct<mapping>(mm::heap, current->addr, current->len, this, current->perms & VMM_LARGE);
        if (current->fault_map) {
            node->fault_map = true;
            node->callbacks = current->callbacks;
        }

        this->create_hole(current->addr, current->len);
        node->perms = current->perms;
        this->mappings.insert(node);

        if (current->huge_page) {
            for (void *inner = current->addr; inner <= ((char *) current->addr + current->len); inner = (char *) inner + memory::common::page_size_2MB) {
                void *phys = x86::_get2(inner, other);
                x86::_map2(phys, inner, current->perms, this);
            }
        } else {
            for (void *inner = current->addr; inner <= ((char *) current->addr + current->len); inner = (char *) inner + memory::common::page_size) {
                void *phys = x86::_get(inner, other);
                x86::_map(phys, inner, current->perms, this);
            }
        }

        current = other->mappings.successor(current);
    }
}

memory::vmm::vmm_ctx::mapping *memory::vmm::vmm_ctx::get_mapping(void *addr) {
    mapping *current = this->mappings.get_root();
    while (current) {
        if (current->addr <= addr && ((char *) current->addr + current->len) >= addr) {
            return current;
        }

        if (current->addr > addr) {
            current = this->mappings.get_left(current);
        } else {
            current = this->mappings.get_right(current);
        }
    }

    return nullptr;
}

void invlpg(void *virt) {
    asm volatile("invlpg (%0)":: "b"(((uint64_t) virt)) : "memory");
}

void memory::vmm::vmm_ctx::delete_pages(void *addr, size_t len, bool huge_page, bool fault_map, bool free_pages, mapping::callback_obj callbacks) {
    if (huge_page) {
        if (fault_map) {
            for (void *inner = addr; inner <= ((char *) addr + len); inner = (char *) inner + memory::common::page_size_2MB) {
                callbacks.unmap(inner, true, map);
                invlpg(inner);
            }
        } else {
            for (void *inner = addr; inner <= ((char *) addr + len); inner = (char *) inner + memory::common::page_size_2MB) {
                if (free_pages) {
                    void *phys = x86::_get((void *) inner, this);
                    x86::_free(phys, 512);
                }

                x86::_unmap2(inner, (void *) this);
                invlpg(inner);
            }
        }
    } else {
        if (fault_map) {
            for (void *inner = addr; inner <= ((char *) addr + len); inner = (char *) inner + memory::common::page_size) {
                callbacks.unmap(inner, false, map);
                invlpg(inner);
            }
        } else {
            for (void *inner = addr; inner <= ((char *) addr + len); inner = (char *) inner + memory::common::page_size) {
                if (free_pages) {
                    void *phys = x86::_get((void *) inner, this);
                    x86::_free(phys);
                }

                x86::_unmap(inner, (void *) this);
                invlpg(inner);
            }
        }
    }
}

void *memory::vmm::vmm_ctx::delete_mappings(void *addr, uint64_t len) {
    auto [start, end] = split_mappings(addr, len);
    if (!start) {
        return nullptr;
    }

    delete_mappings(addr, len, start, end);
    return addr;
}

frg::tuple<memory::vmm::vmm_ctx::mapping *, memory::vmm::vmm_ctx::mapping *> memory::vmm::vmm_ctx::split_mappings(void *addr, uint64_t len) {
    auto highest = mappings.get_root();
    while (highest) {
        if (auto next = mappings.get_left(highest)) {
            highest = next;
        } else {
            break;
        }
    }

    mapping *start = nullptr;
    mapping *end = nullptr;
    for (auto cur = highest; cur;) {
        if (((char *) cur->addr + cur->len) <= addr) {
            cur = mappings.successor(cur);
            start = cur;
            continue;
        }

        if (cur->addr >= ((char *) addr + len)) {
            if (start) {
                end = cur;
            }
            break;
        }

        if (!start) {
            start = cur;
        }

        auto cur_addr = addr;
        if (cur_addr <= cur->addr) {
            cur_addr = (char *) addr + len;
        }
//                             mapping(void *addr, uint64_t len, void *map, bool huge_page) : addr(addr), len(len), map(map), huge_page(huge_page), fault_map(false), free_pages(false) { };
        if (cur_addr > cur->addr && cur_addr < ((char *) cur->addr + cur->len)) {
            auto left = frg::construct<mapping>(mm::heap, cur->addr, ((char *) cur_addr - (char *) cur->addr), this, cur->huge_page);
            if (cur->fault_map) {
                left->callbacks = cur->callbacks;
                left->fault_map = cur->fault_map;
            }
            left->free_pages = cur->free_pages;
            left->perms = cur->perms;

            uint64_t offset = ((char *) cur_addr - (char *) cur->addr);
            auto right = frg::construct<mapping>(mm::heap, (char *) cur->addr + offset, cur->len - offset, this, cur->huge_page);
            if (cur->fault_map) {
                right->callbacks = cur->callbacks;
                right->fault_map = cur->fault_map;
            }
            right->free_pages = cur->free_pages;
            right->perms = cur->perms;

            mappings.remove(cur);
            mappings.insert(left);
            mappings.insert(right);

            frg::destruct(mm::heap, cur);

            if (start == cur) {
                if (addr < cur_addr) {
                    start = left;
                } else {
                    start = right;
                }
            }

            cur = right;
        } else {
            cur = mappings.successor(cur);
        }
    }

    return {start, end};
}

void memory::vmm::vmm_ctx::delete_mappings(void *addr, uint64_t len, mapping *start, mapping *end) {
    for (auto current = start; current != end;) {
        auto mapping = current;
        current = mappings.successor(current);

        if (mapping->addr >= addr && ((char *) mapping->addr + mapping->len) <= ((char *) addr + len)) {
            delete_hole(mapping->addr, mapping->len);
            delete_pages(mapping->addr, mapping->len, mapping->huge_page, mapping->fault_map, mapping->free_pages, mapping->callbacks);
            mappings.remove(mapping);
            frg::destruct(mm::heap, mapping);
        }
    }
}

void memory::vmm::vmm_ctx::delete_mapping(memory::vmm::vmm_ctx::mapping *node) {
    this->delete_hole(node->addr, node->len);
    if (node->huge_page) {
        if (node->fault_map) {
            for (void *inner = node->addr; inner <= ((char *) node->addr + node->len); inner = (char *) inner + memory::common::page_size_2MB) {
                node->callbacks.unmap(inner, true, node->map);
                invlpg(inner);
            }
        } else {
            for (void *inner = node->addr; inner <= ((char *) node->addr + node->len); inner = (char *) inner + memory::common::page_size_2MB) {
                if (node->free_pages) {
                    void *phys = x86::_get((void *) inner, this);
                    x86::_free(phys, 512);
                }

                x86::_unmap2(inner, (void *) this);
                invlpg(inner);
            }
        }
    } else {
        if (node->fault_map) {
            for (void *inner = node->addr; inner <= ((char *) node->addr + node->len); inner = (char *) inner + memory::common::page_size) {
                node->callbacks.unmap(inner, false, node->map);
                invlpg(inner);
            }
        } else {
            for (void *inner = node->addr; inner <= ((char *) node->addr + node->len); inner = (char *) inner + memory::common::page_size) {
                if (node->free_pages) {
                    void *phys = x86::_get((void *) inner, this);
                    x86::_free(phys);
                }

                x86::_unmap(inner, (void *) this);
                invlpg(inner);
            }
        }
    }

    this->mappings.remove(node);
}