#ifndef COMMON_MM_HPP
#define COMMON_MM_HPP

#include <cstddef>

namespace memory {
    namespace common {
        constexpr size_t virtualBase = 0xFFFF800000000000;
        constexpr size_t kernelBase = 0xFFFFFFFF80000000;

        template<typename T>
        T *offsetVirtual(T *ptr) {
            return ((size_t) ptr > virtualBase) ? ptr : (T *) (((size_t) ptr) + virtualBase);
        }

        template<typename T>
        T *removeVirtual(T *ptr) {
            return ((size_t) ptr > virtualBase) ? (T *) (((size_t) ptr) - virtualBase) : ptr;
        }

        constexpr size_t page_size = 0x1000;
        constexpr size_t page_size_2MB = 0x200000;
        inline size_t page_round(size_t size) {
            if ((size % common::page_size) != 0) {
                return ((size / page_size) * common::page_size) + common::page_size;
            }
            
            return ((size / page_size) * common::page_size);
        }

        template<typename T>
        inline T *page_round(T *address) {
            return (T *) page_round((size_t) address);
        }

        inline size_t page_count(size_t size) {
            return page_round(size) / common::page_size;
        }

        template<typename T>
        struct node {
            private:
                node *_prev;
                node *_next;
            public:
                node() : _prev(nullptr), _next(nullptr) { };

                struct iterator {
                    private:
                        T *_node;
                    public:
                        iterator(T *_node) {
                            this->_node = _node;
                        };

                        T *operator *() {
                            return _node;
                        }

                        T *operator ->() {
                            return _node;
                        }

                        iterator& operator ++() {
                            _node = _node->next();
                            return *this;
                        }

                        iterator& operator ++(int) {
                            _node = _node->next();
                            return *this;
                        }

                        iterator& operator --() {
                            _node = _node->prev();
                            return *this;
                        }

                        iterator& operator --(int) {
                            _node = _node->prev();
                            return *this;
                        }

                        bool operator ==(iterator& other) {
                            return this->_node == other._node;
                        }

                        bool operator ==(std::nullptr_t other) {
                            return _node == nullptr;
                        }
                };

                iterator begin() {
                    return iterator{(T *) this};
                }

                std::nullptr_t end(){
                    return nullptr;
                }

                T *next() {
                    return (T *) _next;
                }

                T *prev() {
                    return (T *) _next;
                }

                void append(T *next) {
                    node<T> *_next = (node<T> *) next;

                    if (!this->_next) {
                        _next->_prev = this;
                        _next->_next = nullptr;
                        this->_next = _next;
                        return;
                    }

                    node<T> *_current = this->_next;
                    while (_current->_next) {
                        _current = _current->_next;
                    }

                    _current->append(next);
                }

                void insert(T *prev, T *next) {
                    node<T> *_prev = (node<T> *) prev;
                    node<T> *_next = (node<T> *) next;

                    if (_prev) {
                        _prev->_next = this;
                        this->_prev = _prev;
                    }

                    if (_next) {
                        _next->_prev = this;
                        this->_next = _next;
                    }
                }

                void insert(node *_prev, node *_next) {
                    if (_prev) {
                        _prev->_next = this;
                        this->_prev = _prev;
                    }

                    if (_next) {
                        _next->_prev = this;
                        this->_next = _next;
                    }
                }

                void remove() {
                    if (_prev && _next) {
                        _prev->_next = _next;
                        _next->_prev = _prev;

                        _next = nullptr;
                        _prev = nullptr;
                    } else if (_prev) {
                        _prev->_next = nullptr;
                        _next = nullptr;
                    } else if (_next) {
                        _next->_prev = nullptr;
                        _prev = nullptr;
                    }
                }
        };
    }
}

#endif