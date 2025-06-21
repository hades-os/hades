#ifndef PRS_ALLOCATOR_HPP
#define PRS_ALLOCATOR_HPP

#include <atomic>
#include <cstddef>
#include <utility>

namespace prs {
    struct allocator;
    
    struct memory_resource {
        private:
            std::atomic_int _count;
        public:
            friend struct allocator;
            memory_resource()
                {}
            virtual ~memory_resource() = 0;
            
            virtual void *allocate(size_t bytes,
                size_t alignment = alignof(std::max_align_t)) = 0;
            virtual void deallocate(void *p) = 0;
    };

    struct allocator {
        private:
            memory_resource *_resource;
        public:
            allocator() = delete;
            allocator(memory_resource *resource) {
                if (resource) {
                    if (resource->_count.fetch_add(1)) {
                        _resource = resource;
                    }
                }
            }

            allocator(allocator const& other) {
                if (other._resource) {
                    if (other._resource->_count.fetch_add(1)) {
                        _resource = other._resource;
                    }
                }                
            }

            allocator(allocator&& other) {
                if (other._resource) {
                    if (other._resource->_count.fetch_add(1)) {
                        _resource = std::move(other._resource);
                    }
                }                   
            }

            void *allocate(size_t bytes,
                size_t alignment = alignof(std::max_align_t)) const {
                return _resource->allocate(bytes, alignment);
            }

            void free(void *p) const { deallocate(p); }
            void deallocate(void *p, size_t _) const { deallocate(p); }
            void deallocate(void *p) const {
                if (!p)
                    return;
                _resource->deallocate(p);
            }

            ~allocator() { 
                if (_resource) {
                    if (!_resource->_count.fetch_sub(1)) {
                        _resource->~memory_resource(); 
                    }
                }
            }
    };
}

#endif