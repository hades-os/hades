#ifndef SLAB_HPP
#define SLAB_HPP

#include <cstddef>
#include <cstdint>
#include <util/lock.hpp>
#include "mm/mm.hpp"

namespace slab {
    struct slab;

    struct cache {
        util::spinlock lock;

        size_t object_size;
        size_t active_slabs;
        size_t pages_per_slab;

        slab *head_empty;
        slab *head_partial;
        slab *head_full;

        cache *next;

        slab *create_slab();
        bool move_slab(slab **new_head, slab **old_head, slab *old);

        slab *get_by_pointer(slab *head, void *ptr);

        void *allocate();
        bool deallocate(void *ptr);

        bool has_object(slab *head, void *ptr);
        bool has_object(void *ptr);
    };

    struct slab {
        size_t free_objects;
        size_t total_objects;

        uint8_t *bitmap;
        void *buffer;

        cache *owner;
        slab *next;
        slab *prev;

        void *allocate();
        bool deallocate(void *ptr);
    };

    cache *create(size_t object_size);
    cache *get_by_pointer(void *ptr);

    struct allocator: mm::allocator {
        private:
            cache *root_cache;
        public:
            void *allocate(size_t size);
            void *reallocate(void *ptr, size_t size);

            void deallocate(void *ptr);

            allocator(cache *root_cache): root_cache(root_cache) {}
    };
};

#endif