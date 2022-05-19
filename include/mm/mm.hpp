#ifndef MM_HPP
#define MM_HPP

#include <new>
#include <cstddef>
#include <mm/common.hpp>

namespace memory {
    namespace mm {
        namespace allocator {
            void *malloc(size_t req_size);
            void *realloc(void *p, size_t size);
            void *calloc(size_t nr_items, size_t size);
            void free(void *ptr);
        };
        
        struct heap_allocator {
            void *allocate(size_t size) {
                return allocator::malloc(size);
            }
            
            void deallocate(void *ptr) {
                allocator::free(ptr);
            }

            void deallocate(void *ptr, size_t _) {
                allocator::free(ptr);
            }

            void free(void *ptr) {
                allocator::free(ptr);
            }
        };

        inline mm::heap_allocator heap{};
    };
}

inline void *kmalloc(size_t length) {
    return memory::mm::allocator::malloc(length);
}

inline void *krealloc(void *ptr, size_t length) {
    return memory::mm::allocator::realloc(ptr, length);
}

inline void *kcalloc(size_t nr_items, size_t size) {
    return memory::mm::allocator::calloc(nr_items, size);
}

inline void kfree(void *ptr) {
    memory::mm::allocator::free(ptr);
}

#endif