#ifndef MM_HPP
#define MM_HPP

#include <new>
#include <cstddef>
#include <mm/common.hpp>
#include <type_traits>

namespace mm {
    struct allocator {
        void *allocate(size_t size);
        void *reallocate(void *ptr, size_t size);

        void deallocate(void *ptr);
    };

    allocator boot();
    allocator heap();
    allocator slab(size_t object_size);

    template<auto object, class T=std::decay<decltype(*object)>>
    allocator slab() {
        return slab(sizeof(T));
    }
};

#endif