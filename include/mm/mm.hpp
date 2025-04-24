#ifndef MM_HPP
#define MM_HPP

#include <cstdint>
#include <new>
#include <cstddef>
#include <mm/common.hpp>
#include <type_traits>
#include <util/log/panic.hpp>

namespace mm {
    struct allocator {
        virtual void *allocate(size_t size, size_t alignment = 8) const {
            panic("Use of uninitialized allocator!");
            __builtin_unreachable();
        };
        virtual void *reallocate(void *ptr, size_t size) const {
            panic("Use of uninitialized allocator!");
            __builtin_unreachable();
        };
        virtual void deallocate(void *ptr) const {
            panic("Use of uninitialized allocator!");
            __builtin_unreachable();

        }

        void deallocate(void *ptr, size_t _) const {
            deallocate(ptr);
        }         
        void const free(void *ptr) const {
            deallocate(ptr);
        }
    };

    inline size_t calc_padding(uintptr_t base, size_t alignment) {
        size_t mul = (base / alignment) + 1;
        size_t aligned = mul * alignment;
        size_t padding = aligned - base;

        return padding;
    }

    inline size_t calc_padding(uintptr_t base, size_t alignment, size_t header_size) {
        size_t padding = calc_padding(base, alignment);
        size_t needed_space = header_size;
        if (padding < needed_space) {
            needed_space -= padding;
            if (needed_space % alignment > 0) {
                padding += alignment * (1 + (needed_space / alignment));
            } else {
                padding += alignment * (needed_space / alignment);
            }
        }

        return padding;
    }
};

#endif