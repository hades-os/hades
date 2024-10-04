#ifndef COMMON_MM_HPP
#define COMMON_MM_HPP

#include <cstddef>

namespace memory {
    namespace common {
        constexpr size_t virtualBase = 0xFFFF800000000000;
        constexpr size_t kernelBase = 0xFFFFFFFF80000000;

        template<typename T>
        T *offsetVirtual(T *ptr) {
            return ((size_t) ptr > virtualBase) ? ptr : (T *) (((size_t) ptr) + virtualBase);
        }

        template<typename T>
        T *removeVirtual(T *ptr) {
            return ((size_t) ptr > virtualBase) ? (T *) (((size_t) ptr) - virtualBase) : ptr;
        }

        constexpr size_t page_size = 0x1000;
        constexpr size_t page_size_2MB = 0x200000;
        inline size_t page_round(size_t size) {
            if ((size % common::page_size) != 0) {
                return ((size / page_size) * common::page_size) + common::page_size;
            }
            
            return ((size / page_size) * common::page_size);
        }

        template<typename T>
        inline T *page_round(T *address) {
            return (T *) page_round((size_t) address);
        }

        inline size_t page_count(size_t size) {
            return page_round(size) / common::page_size;
        }
    }
}

#endif