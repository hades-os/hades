#ifndef BOOT_HPP
#define BOOT_HPP

#include <cstddef>
#include <mm/mm.hpp>

namespace boot {
    struct allocator: mm::allocator {
        public:
            void *allocate(size_t size, size_t _ = 0) const override;
            void *reallocate(void *ptr, size_t size) const override;

            void deallocate(void *ptr) const override;
            using mm::allocator::deallocate;
            using mm::allocator::free;

            allocator(): mm::allocator() {}
    };
};

namespace mm {
    boot::allocator boot();
}

#endif