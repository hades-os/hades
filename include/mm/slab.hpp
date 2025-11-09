#ifndef SLAB_HPP
#define SLAB_HPP

#include <cstddef>
#include <cstdint>
#include <util/lock.hpp>
#include "mm/mm.hpp"
#include "prs/allocator.hpp"
#include "prs/list.hpp"

namespace slab {
    struct slab_resource;
    struct slab {
        size_t free_objects;
        size_t total_objects;
        
        uint8_t *bitmap;
        void *buffer;

        slab_resource *owner;
        slab *next;
        slab *prev;

        void *allocate();
        bool deallocate(void *ptr);
    };

    static slab_resource *create_resource(size_t object_size);
    template<typename T>
    static slab_resource *create_resource() {
        return create_resource(sizeof(T));
    }

    struct slab_resource: prs::memory_resource {
        private:
            util::spinlock lock;

            size_t object_size;
            size_t active_slabs;
            size_t pages_per_slab;

            slab *head_empty;
            slab *head_partial;
            slab *head_full;

            slab_resource *next;

            slab *create_slab();
            bool move_slab(slab **new_head, slab **old_head, slab *old);

            slab *get_by_pointer(slab *head, void *ptr);

            void *do_allocate();
            bool do_deallocate(void *ptr);

            bool has_object(slab *head, void *ptr);
            bool has_object(void *ptr);

            static slab_resource *create(size_t object_size);
            static slab_resource *get_by_size(size_t object_size);
        public:
            friend slab_resource *create_resource(size_t object_size);
            friend struct slab;

            void *allocate(size_t, size_t) override;
            void deallocate(void *ptr) override;
    };
};

#endif