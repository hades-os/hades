#include <frg/allocation.hpp>
#include <mm/mm.hpp>
#include <mm/pmm.hpp>
#include <mm/vmm.hpp>
#include <util/log/log.hpp>

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

    if (pred && node->addr < pred->addr + pred->len) {
        panic("[VMM] Hole state violated with address ", node->addr, "pagemap ", node->map);
        return false;
    }

    if (sucs && sucs->addr < node->addr + node->len) {
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
                return this->split_hole(current, 0, len);
            }

            current = this->holes.get_right(current);
        }
    } else {
        while (true) {
            if (addr < current->addr) {
                if (!this->holes.get_left(current)) {
                    break;
                }

                current = this->holes.get_left(current);
            } else if (addr >= current->addr + current->len) {
                if (!this->holes.get_right(current)) {
                    break;
                }

                current = this->holes.get_right(current);
            } else {
                break;
            }
        }

        this->split_hole(current, (uint64_t) addr - (uint64_t) current->addr, len);
        return addr;
    }

    return nullptr;
}

uint8_t memory::vmm::vmm_ctx::delete_hole(void *addr, uint64_t len) {
    hole *current = this->holes.get_root();
    hole *pre, *succ;

    if (addr < current->addr) {
        while (current) {
            if (!this->holes.predecessor(current)) {
                pre = current;
                succ = this->holes.successor(current);
                break;
            } else {
                current = this->holes.predecessor(current);
            }
        }
    } else {
        while (current) {
            if (!this->holes.successor(current)) {
                pre = this->holes.predecessor(current);
                succ = current;
                break;
            } else {
                current = this->holes.successor(current);
            }
        }
    }

    if (pre && pre->addr + pre->len == addr && succ && addr + len == succ->addr) {
        hole *cur = frg::construct<hole>(memory::mm::heap, pre->addr, pre->len + len + succ->len, (void *) this);

        this->holes.remove(pre);
        this->holes.remove(succ);
        this->holes.insert(cur);

        frg::destruct(memory::mm::heap, pre);
        frg::destruct(memory::mm::heap, succ);
    } else if (pre && pre->addr + len == addr) {
        hole *cur = frg::construct<hole>(memory::mm::heap, pre->addr, pre->len + len, (void *) this);

        this->holes.remove(pre);
        this->holes.insert(cur);

        frg::destruct(memory::mm::heap, pre);
    } else if (succ && addr + len == succ->addr) {
        hole *cur = frg::construct<hole>(memory::mm::heap, addr, succ->len + len, (void *) this);

        this->holes.remove(succ);
        this->holes.insert(cur);

        frg::destruct(memory::mm::heap, succ);
    } else {
        hole *cur = frg::construct<hole>(memory::mm::heap, addr, len, (void *) this);

        this->holes.insert(cur);
    }
    return 1;
}

void *memory::vmm::vmm_ctx::split_hole(hole *node, uint64_t offset, size_t len) {
    this->holes.remove(node);

    if (offset) {
        hole *pred = frg::construct<hole>(memory::mm::heap, node->addr, offset, (void *) this);
        this->holes.insert(pred);
    }

    if ((offset + len) < node->len) {
        hole *sucs = frg::construct<hole>(memory::mm::heap, node->addr + offset + len, node->len - (offset + len), (void *) this);
        this->holes.insert(sucs);
    }

    void *ret = node->addr;

    frg::destruct(memory::mm::heap, node);

    return ret;
}

uint8_t memory::vmm::vmm_ctx::mapped(void *addr, uint64_t len) {
    mapping *current = this->mappings.get_root();
    if (!current) {
        return 0;
    }

    if (addr < current->addr) {
        while (current) {
            if (addr >= current->addr && addr < current->addr + current->len) {
                return 1;
            }

            current = this->mappings.predecessor(current);
        }
    } else {
        while (current) {
            if (addr >= current->addr && (addr + len) < current->addr + current->len) {
                return 1;
            }

            current = this->mappings.successor(current);
        }
    }

    return 0;
}

void *memory::vmm::vmm_ctx::unmanaged_mapping(void *addr, uint64_t len, uint64_t flags) {
    if (this->mapped(addr, len)) {
        return nullptr;
    }

    void *dst = this->create_hole(addr, len);
    mapping *node = frg::construct<mapping>(memory::mm::heap, dst, len, (void *) this, flags & PG_LARGE);
    node->is_unmanaged = true;
    this->mappings.insert(node);
    return dst;
}

void *memory::vmm::vmm_ctx::create_mapping(void *addr, uint64_t len, uint64_t flags) {
    if (this->mapped(addr, len)) {
        return nullptr;
    }

    void *dst = this->create_hole(addr, len);

    if (flags & PG_LARGE) {
        for (size_t i = 0; i < memory::common::page_count(len) / 512; i++) {
            memory::vmm::x86::_map2(memory::pmm::phys(512), dst + (memory::common::page_size_2MB * i), flags, (void *) this);
        }
    } else {
        for (size_t i = 0; i < memory::common::page_count(len); i++) {
            memory::vmm::x86::_map(memory::pmm::phys(1), dst + (memory::common::page_size * i), flags, (void *) this);
        }
    }

    mapping *node = frg::construct<mapping>(memory::mm::heap, dst, len, (void *) this, flags & PG_LARGE);
    this->mappings.insert(node);
    return dst;
}

void *memory::vmm::vmm_ctx::create_mapping(void *addr, uint64_t len, uint64_t flags, mapping::callback_obj callbacks) {
    if (this->mapped(addr, len)) {
        return nullptr;
    }

    void *dst = this->create_hole(addr, len);

    if (flags & PG_LARGE) {
        for (size_t i = 0; i < memory::common::page_count(len) / 512; i++) {
            memory::vmm::x86::_map2(nullptr, dst + (memory::common::page_size_2MB * i), flags, (void *) this);
        }
    } else {
        for (size_t i = 0; i < memory::common::page_count(len); i++) {
            memory::vmm::x86::_map(nullptr, dst + (memory::common::page_size * i), flags, (void *) this);
        }
    }

    mapping *node = frg::construct<mapping>(memory::mm::heap, dst, len, (void *) this, flags & PG_LARGE, callbacks);
    this->mappings.insert(node);
    return dst;
}

void *memory::vmm::vmm_ctx::delete_mapping(void *addr, uint64_t len) {
    mapping *current = this->mappings.get_root();
    if (!current) {
        return nullptr;
    }

    if (addr < current->addr) {
        while (current) {
            if (addr == current->addr) {
                goto fnd;
            }

            current = this->mappings.predecessor(current);
        }
    } else {
        while (current) {
            if (addr == current->addr) {
                goto fnd;
            }

            current = this->mappings.successor(current);
        }
    }

    return nullptr;

    fnd:;
    this->delete_hole(addr, len);
    for (void *cur_addr = addr; cur_addr < addr + len && current; cur_addr += memory::common::page_size) {
        if (cur_addr >= current->addr && (current->addr + current->len) <= (addr + len)) {
            if (current->is_unmanaged) {
                goto skip_inside;
            }

            if (current->huge_page) {
                if (current->fault_map) {
                    for (void *inner = current->addr; inner < (current->addr + current->len); inner += memory::common::page_size_2MB) {
                        current->callbacks.unmap(inner, true, current->map);
                    }
                } else {
                    for (void *inner = current->addr; inner < (current->addr + current->len); inner += memory::common::page_size_2MB) {
                        x86::_unmap2(inner, (void *) this);
                    }
                }
            } else {
                if (current->fault_map) {
                    for (void *inner = current->addr; inner < (current->addr + current->len); inner += memory::common::page_size) {
                        current->callbacks.unmap(inner, false, current->map);
                    }
                } else {
                    for (void *inner = current->addr; inner < (current->addr + current->len); inner += memory::common::page_size) {
                        x86::_unmap(inner, (void *) this);
                    }
                }
            }

            skip_inside:;
            mapping *prev = current;
            current = this->mappings.successor(prev);
            this->mappings.remove(prev);

            continue;
        }

        if (cur_addr >= current->addr && (current->addr + current->len) >= (addr + len)) {
            if (current->is_unmanaged) {
                goto skip_outside;
            }

            if (current->huge_page) {
                if (current->fault_map) {
                    for (void *inner = current->addr; inner < (addr + len); inner += memory::common::page_size_2MB) {
                        current->callbacks.unmap(inner, true, current->map);      
                    }
                } else {
                    for (void *inner = current->addr; inner < (addr + len); inner += memory::common::page_size_2MB) {
                        x86::_unmap2(inner, (void *) this);
                    }
                }
            } else {
                if (current->fault_map) {
                    for (void *inner = current->addr; inner < (addr + len); inner += memory::common::page_size) {
                        current->callbacks.unmap(inner, false, current->map);      
                    }
                } else {
                    for (void *inner = current->addr; inner < (addr + len); inner += memory::common::page_size) {
                        x86::_unmap(inner, (void *) this);
                    }
                }
            }

            skip_outside:;
            current->addr = addr + len;
            break;
        }
    }

    return addr;
}