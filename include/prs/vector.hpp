#ifndef PRS_VECTOR_HPP
#define PRS_VECTOR_HPP

#include <cstddef>
#include <utility>

#include "prs/assert.hpp"

namespace prs {
    size_t next_pow2(size_t n) {
        if (n == 0)
            return 1;

        size_t leading_bit = 1;
        while (n != 0) {
            n = n >> 1;
            leading_bit = leading_bit << 1;
        }

        return leading_bit;
    }

    template<typename T, typename Allocator>
    struct vector {
        private:
            size_t _size;
            size_t _capacity;
            T *_elements;
            Allocator allocator;

            void ensure_capacity(size_t capacity) {
                if (capacity <= _capacity)
                    return;

                size_t new_capacity = next_pow2(capacity + 1);
                T *new_array = (T *) allocator.allocate(sizeof(T) * new_capacity);
                for (size_t i = 0; i < _capacity; i++) {
                    new (&(new_array[i])) T(std::move(_elements[i]));
                }

                for (size_t i = 0; i < _size; i++) {
                    _elements[i].~T();
                }

                allocator.free(_elements);

                _elements = new_array;
                _capacity = new_capacity;
            }
        public:
            using value_type = T;
            using reference = T &;
            struct iterator {
                private:
                    T *current;
                public:
                    using difference_type = std::ptrdiff_t;
                    using value_type = T;
                    using pointer = T *;

                    iterator(pointer ptr):
                        current(ptr) {}

                    pointer operator *() const {
                        return current;
                    }

                    pointer operator->() {
                        return current;
                    }

                    iterator& operator++() {
                        current++;
                        return *this;
                    }

                    iterator operator++(int) {
                        auto copy = *this;
                        ++(*this);
                        return copy;
                    }

                    friend bool operator==(const iterator& a, const iterator& b) {
                        return a.current == b.current;
                    }

                    friend bool operator!=(const iterator& a, const iterator& b) {
                        return a.current != b.current;
                    }
            };

            struct reverse_iterator {
                private:
                    friend struct list;
                    T *current;
                public:
                    using difference_type = std::ptrdiff_t;
                    using value_type = T;
                    using pointer = T *;

                    reverse_iterator(pointer ptr):
                        current(ptr) {}

                    pointer operator *() const {
                        return current;
                    }

                    pointer operator->() {
                        return current;
                    }

                    iterator& operator++() {
                        current--;
                        return *this;
                    }

                    iterator operator++(int) {
                        auto copy = *this;
                        ++(*this);
                        return copy;
                    }

                    friend bool operator==(const reverse_iterator& a, 
                            const reverse_iterator& b) {
                        return a.current == b.current;
                    }

                    friend bool operator!=(const reverse_iterator& a, 
                            const reverse_iterator& b) {
                        return a.current != b.current;
                    }
            };

            vector(Allocator allocator = Allocator()):
                _size(0), _capacity(0), _elements(nullptr), allocator(std::move(allocator)) {}

            vector(const vector& other):
                allocator(other.allocator) {
                size_t other_size = other.size();
                ensure_capacity(other_size);
                for (size_t i = 0; i < other_size; i++) {
                    new (&_elements[i]) T(other[i]);
                }

                _size = other_size;
            }

            vector(vector&& other) {
                swap(*this, other);
            }

            vector &operator=(vector other) {
                swap(*this, other);
                return *this;
            }
            
            ~vector() {
                for (size_t i = 0; i < _size; i++) {
                    _elements[i].~T();
                }

                allocator.free(_elements);
            }

            friend void swap(vector& a, vector& b) {
                std::swap(a.allocator, b.allocator);
                std::swap(a._elements, b._elements);
                std::swap(a._size, b._size);
                std::swap(a._capacity, b._capacity);
            }

            const T &operator[](size_t index) const {
                prs::assert(index < _size);
                return _elements[index];
            }
            
            T &operator[](size_t index) {
                prs::assert(index < _size);
                return _elements[index];
            }

            T &front() {
                return _elements[0];
            }
            
            const T &front() const {
                return _elements[0];
            }
        
            T &back() {
                return _elements[_size - 1];
            }

            const T &back() const {
                return _elements[_size - 1];
            }

            T *data() {
                return _elements;
            }
        
            const T *data() const {
                return _elements;
            }

            iterator begin() {
                return iterator{_elements};
            }

            iterator end() {
                return iterator{_elements + _size};
            }

            reverse_iterator rbegin() {
                return reverse_iterator{_elements + _size};
            }

            reverse_iterator rend() {
                return reverse_iterator{_elements};
            }

            bool empty() {
                return _size == 0;
            }

            size_t size() {
                return _size;
            }

            
    }
}

#endif