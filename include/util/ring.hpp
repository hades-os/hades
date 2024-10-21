#ifndef RING_HPP
#define RING_HPP

#include <cstddef>
namespace util {

    template <typename T>
    class ring {
        private:
            T *data;
            size_t size;
            size_t head;
            size_t tail;
        public:
            size_t items;

            ring(size_t size);

            void push(T obj);
            T pop();
            T pop_back();
            const T peek();
    };
}

#endif