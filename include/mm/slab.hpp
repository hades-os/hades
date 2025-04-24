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
    cache *get_by_size(size_t object_size);

    struct allocator: mm::allocator {
        private:
            size_t object_size;
            mutable cache *base_cache;
        public:
            void *allocate(size_t, size_t _ = 0) const override;
            void deallocate(void *ptr) const override;
            
            using mm::allocator::deallocate;
            using mm::allocator::free;

            allocator(size_t object_size, cache *base_cache): mm::allocator(), object_size(object_size), base_cache(base_cache) {}
            allocator(const allocator& other):
                mm::allocator(),
                object_size(other.object_size), base_cache(other.base_cache) {}
    };
};

namespace mm {
    slab::allocator slab(size_t object_size);

    template<typename T>
    slab::allocator slab() {
        return slab(sizeof(T));
    }    
}

#endif