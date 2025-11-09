#ifndef PRS_ALLOCATOR_HPP
#define PRS_ALLOCATOR_HPP

#include <atomic>
#include <cstddef>
#include <utility>
#include "prs/assert.hpp"

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
            virtual void *reallocate(void *p, size_t new_bytes) = 0;
    };

    struct allocator {
        private:
            memory_resource *_resource;
        public:
            allocator():
                _resource(nullptr) {}
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

            allocator& operator=(allocator& other) {
                if (other._resource) {
                    if (other._resource->_count.fetch_add(1)) {
                        _resource = other._resource;
                    }
                }
            }

            void *allocate(size_t bytes,
                size_t alignment = alignof(std::max_align_t)) const {
                prs::assert(_resource != nullptr);

                return _resource->allocate(bytes, alignment);
            }

            void free(void *p) const { deallocate(p); }
            void deallocate(void *p, size_t _) const { deallocate(p); }
            void deallocate(void *p) const {
                if (!p)
                    return;

                prs::assert(_resource != nullptr);
                _resource->deallocate(p);
            }

            void *reallocate(void *p, size_t new_bytes) {
                if (!p)
                    return _resource->allocate(new_bytes);

                prs::assert(_resource != nullptr);
                return _resource->reallocate(p, new_bytes);
            }

            void swap(allocator& other) {
                using std::swap;

                swap(_resource, other._resource);
            }
            
            ~allocator() { 
                if (_resource) {
                    if (!_resource->_count.fetch_sub(1)) {
                        _resource->~memory_resource(); 
                    }
                }
            }
    };

    void swap(allocator& lhs, allocator& rhs) {
        lhs.swap(rhs);
    }
}

#endif