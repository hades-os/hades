#ifndef PRS_DELETER_HPP
#define PRS_DELETER_HPP

#include <utility>
namespace prs {
    template<typename T>
    struct deleter {
        virtual void operator()(T *ptr) = 0;
    };

    template<typename T, typename Allocator>
    struct default_deleter: deleter<T> {
        private:
            Allocator alloc;
        public:
            constexpr default_deleter() noexcept = default;
            
            default_deleter(Allocator alloc):
                alloc(alloc) {}

            default_deleter(const default_deleter& other):
                alloc(std::move(other.alloc)) {}

            template<typename U>
            default_deleter(const default_deleter<U, Allocator>& other):
                alloc(std::move(other.alloc)) {}

            void operator()(T *ptr) override {
                ptr->~T();
                alloc.free(ptr);
            }
    };    
}

#endif