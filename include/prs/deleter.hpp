#ifndef PRS_DELETER_HPP
#define PRS_DELETER_HPP

namespace prs {
    template<typename U>
    struct deleter {
        virtual void operator()(U *ptr) = 0;
    };

    template<typename U, typename Allocator>
    struct default_deleter: deleter<U> {
        private:
            Allocator alloc;
        public:
            default_deleter(Allocator alloc):
                alloc(alloc) {}

            void operator()(U *ptr) {
                alloc.free(ptr);
            }
    };    
}

#endif