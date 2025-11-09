#ifndef PRS_BOX_HPP
#define PRS_BOX_HPP

#include <new>
#include <type_traits>
#include <utility>
#include "prs/assert.hpp"

namespace prs {
    template<typename T>
    struct box {
        public:
            box(): 
                initialized(false) {}

            template<typename... Args>
            void construct(Args&& ...args) {
                prs::assert(!initialized);
                new(&storage) T(std::forward<Args>(args)...);
                initialized = true;
            }

            template<typename F, typename... Args>
            void construct_with(F f) {
                prs::assert(!initialized);
                new(&storage) T{f()};
                initialized = true;
            }

            void destruct() {
                prs::assert(initialized);
                get()->T::~T();
                initialized = false;
            }

            T *get() {
                prs::assert(initialized);
                return std::launder(reinterpret_cast<T *>(&storage));
            }
        
            bool valid() {
                return initialized;
            }
        
            operator bool () {
                return initialized;
            }
        
            T *operator-> () {
                return get();
            }
            T &operator* () {
                return *get();
            }            
        private:
            std::aligned_storage<sizeof(T), alignof(T)> storage;
            bool initialized;
    };
}

#endif