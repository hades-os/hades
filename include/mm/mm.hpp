#ifndef MM_HPP
#define MM_HPP

#include <new>
#include <cstddef>
#include <mm/common.hpp>

namespace mm {
    struct allocator {
        void *allocate(size_t size);
        void *reallocate(void *ptr, size_t size);

        void deallocate(void *ptr);
    };

    allocator boot();
    allocator heap();
    allocator slab(size_t object_size);

    template<typename T>
    allocator slab() {
        return slab(sizeof(T));
    }
};

#endif