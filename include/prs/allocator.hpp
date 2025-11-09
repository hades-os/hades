#ifndef PRS_ALLOCATOR_HPP
#define PRS_ALLOCATOR_HPP

#include <cstddef>
#include <tuple>

namespace prs {
    struct memory_resource;
    struct allocator;

    struct memory_resource {
        private:
            std::tuple<void *, size_t>
        public:
            friend struct allocator;

            memory_resource()
                {}

            virtual ~memory_resource() = 0;
            
            virtual void *allocate(size_t bytes,
                size_t alignment = alignof(std::max_align_t)) = 0;
            virtual void deallocate(void *p) = 0;
            virtual bool is_equal(memory_resource& other) = 0;
    }
}

#endif