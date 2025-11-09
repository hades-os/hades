#ifndef SLAB_HPP
#define SLAB_HPP

#include <cstddef>
#include <cstdint>
#include <util/lock.hpp>
#include "mm/mm.hpp"
#include "prs/allocator.hpp"
#include "prs/list.hpp"

namespace slab {
    struct cache;
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

    struct slab_resource;
    struct cache {
        private:
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

            bool has_object(slab *head, void *ptr);
            bool has_object(void *ptr);
        public:
            friend struct slab;
            friend struct slab_resource;

            void *do_allocate();
            bool do_deallocate(void *ptr);

            static cache *create(size_t object_size);
            static cache *get_by_size(size_t object_size);
    };

    struct slab_resource: prs::memory_resource {
        public:
            friend slab_resource *create_resource(size_t object_size);
            friend struct slab;

            ~slab_resource();

            void *allocate(size_t size, size_t
                alignment = alignof(std::max_align_t)) override;
            void deallocate(void *ptr) override;       
            void *reallocate(void *p, size_t new_bytes) override; 
    };

    slab_resource *create_resource();
};

#endif