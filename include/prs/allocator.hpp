#ifndef PRS_ALLOCATOR_HPP
#define PRS_ALLOCATOR_HPP

#include <cstddef>

namespace prs {
    struct allocator;

    namespace _prs {
        struct memory_resource {
            memory_resource()
                {}
            virtual ~memory_resource() = 0;
            
            virtual void *allocate(size_t bytes,
                size_t alignment = alignof(std::max_align_t)) = 0;
            virtual void deallocate(void *p) = 0;
            virtual bool is_equal(memory_resource& other) = 0;
        };
    }

    template<typename T>
    struct memory_resource: public _prs::memory_resource {
        public:
            friend struct allocator;

            memory_resource()
                {}

            static T *create_resource() {
                return T::create_resource();
            }

            static void delete_resource(T *resource) {
                T::delete_resource(resource);
            }
    };

    struct allocator {
        private:
            _prs::memory_resource *_resource;
        public:
            void *allocate(size_t bytes,
                size_t alignment = alignof(std::max_align_t)) {
                return _resource->allocate(bytes, alignment);
            }

            void free(void *p) { deallocate(p); }
            void deallocate(void *p) {
                _resource->deallocate(p);
            }

            ~allocator()
                { _resource->~memory_resource(); }
    };
}

#endif