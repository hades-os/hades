#ifndef PRS_CONSTRUCT_HPP
#define PRS_CONSTRUCT_HPP

#include <cstddef>
#include <utility>
namespace prs {
    template<typename T, typename Allocator, typename... Args>
    T *construct(const Allocator &allocator, Args &&... args) {
        void *memory = allocator.allocate(sizeof(T));
        return new(memory) T(std::forward<Args>(args)...);
    }

    template<typename T, typename Allocator, typename... Args>
    T *construct_n(Allocator &allocator, size_t n, Args &&... args) {
        T *memory = (T *)allocator.allocate(sizeof(T) * n);
        for(size_t i = 0; i < n; i++)
            new(&memory[i]) T(std::forward<Args>(args)...);
        return memory;
    }

    template<typename T, typename Allocator>
    void destruct(const Allocator &allocator, T *object) {
        if(!object)
            return;
        object->~T();
        allocator.deallocate(object, sizeof(T));
    }

    template<typename T, typename Allocator>
    void destruct_n(Allocator &allocator, T *array, size_t n) {
        if(!array)
            return;
        for(size_t i = 0; i < n; i++)
            array[i].~T();
        allocator.deallocate(array, sizeof(T) * n);
    }
}

#endif