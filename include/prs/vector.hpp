#ifndef PRS_VECTOR_HPP
#define PRS_VECTOR_HPP

#include <algorithm>
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
            using difference_type = std::ptrdiff_t;
            struct iterator {
                private:
                    T *current;
                public:
                    using value_type = T;
                    using pointer = T *;

                    iterator(pointer ptr):
                        current(ptr) {}

                    reference operator *() const {
                        return *current;
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

                    explicit operator bool() {
                        return current != nullptr;
                    }

                    friend bool operator==(const iterator& a, const iterator& b) {
                        return a.current == b.current;
                    }

                    friend bool operator!=(const iterator& a, const iterator& b) {
                        return a.current != b.current;
                    }

                    friend iterator operator+(size_t n, const iterator& a) {
                        return iterator{a.current + n};
                    }

                    friend iterator operator-(size_t n, const iterator& a) {
                        return iterator{a.current - n};
                    }

                    friend difference_type operator-(const iterator& a, const iterator& b) {
                        return a.current - b.current;
                    }
            };

            struct reverse_iterator {
                private:
                    T *current;
                public:
                    using value_type = T;
                    using pointer = T *;

                    reverse_iterator(pointer ptr):
                        current(ptr) {}

                    reference operator *() const {
                        return *current;
                    }

                    pointer operator->() {
                        return current;
                    }

                    reverse_iterator& operator++() {
                        current--;
                        return *this;
                    }

                    reverse_iterator operator++(int) {
                        auto copy = *this;
                        ++(*this);
                        return copy;
                    }

                    friend bool operator==(const reverse_iterator& a, 
                            const reverse_iterator& b) {
                        return a.current == b.current;
                    }

                    explicit operator bool() {
                        return current != nullptr;
                    }

                    friend bool operator!=(const reverse_iterator& a, 
                            const reverse_iterator& b) {
                        return a.current != b.current;
                    }

                    friend reverse_iterator operator+(size_t n, const reverse_iterator& a) {
                        return iterator{a.current + n};
                    }

                    friend reverse_iterator operator-(size_t n, const reverse_iterator& a) {
                        return reverse_iterator{a.current - n};
                    }

                    friend difference_type operator-(const reverse_iterator& a, const reverse_iterator& b) {
                        return a.current - b.current;
                    }                    
            };

            vector():
                _size(0), _capacity(0), _elements(nullptr) {}

            vector(Allocator allocator):
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

            vector(vector&& other):
                allocator(std::move(other.allocator)), 
                _elements(std::move(other._elements)),
                _size(std::move(other._size)), _capacity(std::move(other._capacity)) {
            }

            vector &operator=(vector other) {
                using std::swap;

                swap(*this, other);
                return *this;
            }
            
            ~vector() {
                for (size_t i = 0; i < _size; i++) {
                    _elements[i].~T();
                }

                allocator.free(_elements);
            }

            void swap(vector& other) {
                using std::swap;
                swap(allocator, other.allocator);
                swap(_elements, other._elements);
                swap(_size, other._size);
                swap(_capacity, other._capacity);
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

            bool empty() const {
                return _size == 0;
            }

            size_t size() const {
                return _size;
            }

            size_t capacity() const {
                return _capacity;
            }

            template <typename... Args>
            void resize(size_t new_size, Args&& ...args) {
                ensure_capacity(new_size);
                if (new_size < _size) {
                    for (size_t i = new_size; i < _size; i++) {
                        _elements[i].~T();
                    }
                } else {
                    for (size_t i = _size; i < new_size; i++) {
                        new (&_elements[i]) T(std::forward<Args>(args)...);
                    }
                }

                _size = new_size;
            }

            void clear() {
                for (size_t i = 0; i < _size; i++) {
                    _elements[i].~T();
                }

                _size = 0;
            }

            T& push_back(const T& element) {
                ensure_capacity(_size + 1);
                T *p = new(&_elements[_size]) T(element);
                _size++;
                return *p;
            }

            T& push_back(T&& value) {
                ensure_capacity(_size + 1);
                T *p = new(&_elements[_size]) T(std::move(value));
                _size++;
                return *p;
            }

            T pop_back() {
                _size--;
                T element = std::move(_elements[_size]);
                _elements[_size].~T();

                return element;
            }

            template<typename... Args>
            T &emplace_back(Args&& ...args) {
                ensure_capacity(_size + 1);
                T *p = new(&_elements[_size]) T(std::forward<Args>(args)...);
                _size++;
                return *p;
            }

            iterator insert(iterator pos, T&& value) {
                difference_type diff = pos - begin();
                prs::assert(diff > 0 && diff < _size);

                size_t current = static_cast<size_t>(diff);
                if (current >= _capacity) {
                    ensure_capacity(_size + 1);
                }

                for (size_t i = _size; i > current; i--) {
                    _elements[i + 1] = _elements[i];
                }

                new(&_elements[current]) T(std::move(value));
                _size++;
                return iterator{_elements + current};
            }

            iterator insert(iterator pos, const T& element) {
                difference_type diff = pos - begin();
                prs::assert(diff > 0 && diff < _size);

                size_t current = static_cast<size_t>(diff);
                if (current >= _capacity) {
                    ensure_capacity(_size + 1);
                }

                for (size_t i = _size; i > current; i--) {
                    _elements[i + 1] = _elements[i];
                }

                new(&_elements[current]) T(element);
                _size++;
                return iterator{_elements + current};         
            }

            template<typename... Args>
            iterator emplace(iterator pos, Args&&... args) {
                difference_type diff = pos - begin();
                prs::assert(diff > 0 && diff < _size);

                size_t current = static_cast<size_t>(diff);
                if (current >= _capacity) {
                    ensure_capacity(_size + 1);
                }

                for (size_t i = _size; i > current; i--) {
                    _elements[i + 1] = _elements[i];
                }

                new(&_elements[current]) T(std::forward<Args>(args)...);
                _size++;
                return iterator{_elements + current};         
            }

            iterator erase(T element) {
                iterator pos{nullptr};
                for (iterator it = begin(); it != end(); it++) {
                    if (*it == element) {
                        pos = it;
                        break;
                    }
                }

                if (!pos) {
                    return iterator{nullptr};
                }

                return erase(pos);          
            }

            iterator erase(iterator pos) {
                auto const res = iterator{_elements + (pos - _elements)};
                auto const endIter = end();

                for (auto p = res; p != endIter;) {
                    auto& lhs = *p;
                    ++p;
                    lhs = std::move(*p);
                }

                --_size;
                return res;
            }

            iterator erase(iterator first, iterator last) {
                auto const res = iterator{_elements + (first - _elements)};
                if (first == last)
                    return res;

                auto writeIter = res;
                auto readIter = iterator{_elements + (last - _elements)};

                for (auto const endIter = end(); readIter != endIter;
                    ++writeIter, ++readIter) {
                    *writeIter = std::move(*readIter);
                }

                _size = (writeIter - _elements);
            }
    };

    template<typename T, typename Allocator>
    void swap(vector<T, Allocator>& lhs, vector<T, Allocator>& rhs) {
        lhs.swap(rhs);
    }
}

#endif