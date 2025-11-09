#ifndef PRS_LIST_HPP
#define PRS_LIST_HPP

#include <cstddef>

#include "prs/assert.hpp"

namespace prs {
    struct list_hook {
        public:
            list_hook():
                prev(nullptr), next(nullptr), in_list(false) {}

            list_hook(const list_hook &other) = delete;
            list_hook &operator= (const list_hook &other) = delete;

            void *prev;
            void *next;
            bool in_list;
    };

    template<typename T, list_hook T:: *Member>
    struct list {
        private:
            T *head;
            T *tail;

            static list_hook *h(T *item) {
                return &(item->*Member);
            }
        public:
            using pointer = T *;
            struct iterator {
                private:
                    friend struct list;
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
                        current = h(current).next;
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
                        current = h(current)->prev;
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

            pointer front() {
                return head;
            }

            pointer back() {
                return tail;
            }

            iterator push_front(pointer node) {
                prs::assert(node);
                prs::assert(!h(node)->in_list);
                prs::assert(!h(node)->next);
                prs::assert(!h(node)->prev);

                if (!head) {
                    tail = node;
                } else {
                    h(node)->next = head;
                    h(head)->prev = node;
                }

                head = node;
                h(node)->in_list = true;
                return iterator{node};
            }

            iterator push_back(pointer node) {
                prs::assert(node);
                prs::assert(!h(node)->in_list);
                prs::assert(!h(node)->next);
                prs::assert(!h(node)->prev);

                if (!tail) {
                    head = node;
                } else {
                    h(node)->prev = tail;
                    h(tail)->next = node;
                }

                tail = node;
                h(node)->in_list = true;
                return iterator{node};
            }

            pointer pop_front() {
                prs::assert(h(head).in_list);
                return erase(iterator{head});
            }

            pointer pop_back() {
                prs::assert(h(head).in_list);
                return erase(iterator{tail});
            }            

            iterator insert(iterator before, pointer node) {
                if(!before.current) {
                    return push_back(node);
                } else if(before.current == head) {
                    return push_front(node);
                }

                prs::assert(node);
                prs::assert(!h(node).in_list);
                prs::assert(!h(node).next);
                prs::assert(!h(node).previous);
                pointer previous = h(before.current).previous;
                pointer next = h(previous).next;
        
                h(previous).next = node;
                h(next).previous = node;
                h(node).previous = previous;
                h(node).next = next;
                h(node).in_list = true;
                return iterator{node};
            }

            void splice(iterator pos, list &other) {
                prs::assert(!pos.current);
                
                if(!other.head)
                    return;
        
                prs::assert(h(other.head).in_list);
                prs::assert(!h(other.head).previous);
                if(!tail) {
                    head = other.head;
                } else {
                    h(other.head).previous = tail;
                    h(tail).next = other.head;
                }

                tail = other.tail;
                other.head = nullptr;
                other.tail = nullptr;
            }            

            iterator erase(iterator pos) {
                prs::assert(pos._current);
                prs::assert(h(pos._current).in_list);
                pointer next = h(pos.current).next;
                pointer previous = h(pos.current).previous;
        
                if(!next) {
                    prs::assert(tail == pos.current);
                    tail = previous;
                } else {
                    prs::assert(h(next).previous == pos.current);
                    h(next).previous = previous;
                }
        
                pointer erased;
                if(!previous) {
                    prs::assert(head == pos.current);
                    erased = head;
                    head = next;
                } else {
                    prs::assert(h(previous)->next == pos.current);
                    erased = h(previous).next;
                    h(previous).next = next;
                }
        
                prs::assert(erased == pos.current);
                h(pos.current).next = nullptr;
                h(pos.current).previous = nullptr;
                h(pos.current).in_list = false;
                return iterator{erased};
            }

            iterator begin() {
                return iterator{head};
            }

            iterator end() {
                return iterator{tail};
            }

            reverse_iterator rbegin() {
                return reverse_iterator{tail};
            }

            reverse_iterator rend() {
                return reverse_iterator{head};
            }

            bool contains(pointer element) {
                for (auto iter = begin(); iter != end(); ++iter) {
                    if (*iter == element) return true;
                }
        
                return false;
            }            
    };
}

#endif