#include <stdbool.h>
#include <cstddef>
#include <cstdint>
#include <mm/common.hpp>
#include <mm/slab.hpp>
#include <mm/pmm.hpp>
#include <util/misc.hpp>
#include <util/string.hpp>
#include <util/log/panic.hpp>
#include <util/lock.hpp>

constexpr size_t slab_max_objects = 512;
slab::cache *root_cache;

slab::slab *slab::cache::create_slab() {
    slab *new_slab = (slab *) pmm::alloc(pages_per_slab);

    new_slab->bitmap = (uint8_t *) ((uintptr_t) new_slab + sizeof(slab));
    new_slab->buffer = (void *) (util::align((uintptr_t) new_slab->bitmap + slab_max_objects, 16));
    new_slab->free_objects = slab_max_objects;
    new_slab->total_objects = slab_max_objects;
    new_slab->owner = this;

    if (head_empty)
        head_empty->prev = new_slab;
    
    new_slab->next = head_empty;
    head_empty = new_slab;

    return new_slab;
}

bool slab::cache::move_slab(slab **new_head, slab **old_head, slab *old) {
    if (!old || !old_head)
        return false;

    if (old->next)
        old->next->prev = old->prev;
    if (old->prev)
        old->prev->next = old->next;
    if (*old_head == old)
        *old_head = old->next;

    if (!*new_head) {
        old->prev = nullptr;
        old->next = nullptr;
        *new_head = old;
        return true;
    }

    old->next = *new_head;
    old->prev = nullptr;

    if (*new_head)
        (*new_head)->prev = old;

    *new_head = old;
    return true;      
}

slab::slab *slab::cache::get_by_pointer(slab *head, void *ptr) {
    slab *current = head;
    while (current) {
        if (current->buffer <= ptr && (void *) ((uintptr_t) current->buffer + object_size * current->total_objects) > ptr) {
            return current;
        }

        current = current->next;
    }

    return nullptr;
}

void *slab::slab::allocate() {
    for (size_t i = 0; i < total_objects; i++) {
        if (!util::bit_test(bitmap, i)) {
            util::bit_set(bitmap, i);
            free_objects--;

            void *addr = (void *) ((uintptr_t) buffer + (i * owner->object_size));
            memset(addr, 0, owner->object_size);
            return addr;
        }
    }

    // Possible OOM?
    panic("[SLAB]: Out of memory!");
    __builtin_unreachable();
}

bool slab::slab::deallocate(void *ptr) {
    size_t index = ((uintptr_t) ptr - (uintptr_t) buffer) / owner->object_size;
    if (util::bit_test(bitmap, index)) {
        util::bit_clear(bitmap, index);
        free_objects++;
        return true;
    }

    return false;
}

bool slab::cache::deallocate(void *ptr) {
    // TODO: free empty slabs, memory pressure maybe?

    slab *slab;

    if ((slab = get_by_pointer(head_partial, ptr)))
        return slab->deallocate(ptr);
    else if((slab = get_by_pointer(head_full, ptr)))
        return slab->deallocate(ptr);

    return false;
}

void *slab::cache::allocate() {
    util::lock_guard guard{lock};

    slab *slab = nullptr;

    if (head_partial)
        slab = head_partial;
    else if(head_empty)
        slab = head_empty;

    if (!slab) {
        slab = create_slab();
        head_empty = slab;
    }

    void *addr = slab->allocate();
    if (slab->free_objects == 0)
        move_slab(&head_full, &head_partial, slab);
    else if (slab->free_objects == (slab->total_objects - 1))
        move_slab(&head_partial, &head_empty, slab);

    return addr;
}

bool slab::cache::has_object(slab *head, void *ptr) {
    if (!head)
        return false;

    util::lock_guard guard{lock};
    
    slab *current = head;
    while (current) {
        if (current->buffer <= ptr && (void *) ((uintptr_t) current->buffer + object_size * current->total_objects) > ptr) {
            return true;
        }

        current = current->next;
    }

    return false;
}

bool slab::cache::has_object(void *ptr) {
    if (has_object(head_partial, ptr))
        return true;
    
    if (has_object(head_full, ptr))
        return true;

    return false;
}

slab::cache *slab::get_by_size(size_t object_size) {
    cache *current = root_cache;
    while (current) {
        if (current->object_size == object_size)
            return current;

        current = current->next;
    }

    return nullptr;
}

slab::cache *slab::create(size_t object_size) {
    cache tmp{};

    tmp.pages_per_slab = util::ceil(object_size * slab_max_objects + sizeof(slab) + slab_max_objects, memory::page_size);
    tmp.object_size = object_size;
    
    slab *root = tmp.create_slab();
    cache *new_cache = (cache *) root->buffer;

    new_cache->pages_per_slab = tmp.pages_per_slab;
    new_cache->object_size = object_size;

    root->owner = new_cache;
    root->buffer = (void *) ((uintptr_t) root->buffer + sizeof(cache));
    root->free_objects -= util::ceil(object_size,sizeof(cache));
    root->total_objects = root->free_objects;

    new_cache->head_empty = root;
    new_cache->next = root_cache;

    root_cache = new_cache;

    return new_cache;
}

void *slab::allocator::allocate(size_t, size_t _) const {
    if (!base_cache)
        base_cache = create(object_size);

    return base_cache->allocate();
}

void slab::allocator::deallocate(void *ptr) const {
    if (!ptr)
        return;
    
    base_cache->deallocate(ptr);
}

slab::allocator mm::slab(size_t object_size) {
    return slab::allocator(object_size, slab::get_by_size(object_size));
}