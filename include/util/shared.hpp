#ifndef SHAREDPTR_HPP
#define SHAREDPTR_HPP

#include <cstddef>
#include <frg/allocation.hpp>
#include <mm/mm.hpp>

namespace util {
    namespace {
        class counter {
            private:
                size_t _counter;
            public:
                counter() : _counter(0) { };
                counter(const counter& ) = delete;
                counter& operator=(const counter& ) = delete;

                ~counter() {};

                void reset() {
                    _counter = 0;
                }

                size_t get() {
                    return _counter;;
                }

                void operator ++(int) {
                    _counter++;
                }

                void operator --(int) {
                    _counter--;
                }
        };
    };

    template<typename T>
    class shared_ptr {
        private:
            counter *_counter;
            T *_pointer;
        public:
            explicit shared_ptr(T *ptr = nullptr) {
                _pointer = ptr;
                _counter = frg::construct<counter>(memory::mm::heap);
                if (ptr) {
                    (*_counter)++;
                }
            }

            shared_ptr(shared_ptr<T>& other) {
                _pointer = other._pointer;
                _counter = other._counter;
                (*_counter)++;
            }

            size_t uses() {
                return _counter->get();
            }

            T& operator *() {
                return *_pointer;
            }

            T *operator ->() {
                return _pointer;
            }

            ~shared_ptr() {
                (*_counter)--;
                if (!_counter->get()) {
                    frg::destruct(memory::mm::heap, _counter);
                    frg::destruct(memory::mm::heap, _pointer);
                }
            }
    };
};

#endif