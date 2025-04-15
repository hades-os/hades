#ifndef BOOT_HPP
#define BOOT_HPP

#include <cstddef>
namespace boot {
    struct allocator {
        public:
            void *allocate(size_t size);
            void *reallocate(void *ptr, size_t size);

            void deallocate(void *ptr);
    };
};

#endif