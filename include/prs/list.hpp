#ifndef PRS_LIST_HPP
#define PRS_LIST_HPP

#include <cstddef>

#include "prs/assert.hpp"

namespace prs {
    struct list_hook {
        public:
            list_hook():
                previous(nullptr), next(nullptr), in_list(false) {}

            list_hook(const list_hook &other) = delete;
            list_hook &operator= (const list_hook &other) = delete;

            void *previous;
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
                        current = static_cast<pointer>(h(current)->next);
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
                        current = static_cast<pointer>(h(current)->previous);
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

                    friend bool operator==(const reverse_iterator& a, 
                            const reverse_iterator& b) {
                        return a.current == b.current;
                    }

                    friend bool operator!=(const reverse_iterator& a, 
                            const reverse_iterator& b) {
                        return a.current != b.current;
                    }
            };

            list():
                head(nullptr), tail(nullptr) {}

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
                prs::assert(!h(node)->previous);

                h(node)->next = head;
                if (!head) {
                    tail = node;
                } else {
                    h(head)->previous = node;
                }

                head = node;
                h(node)->in_list = true;
                return iterator{node};
            }

            iterator push_back(pointer node) {
                prs::assert(node);
                prs::assert(!h(node)->in_list);
                prs::assert(!h(node)->next);
                prs::assert(!h(node)->previous);

                h(node)->previous = tail;
                if (!tail) {
                    head = node;
                } else {
                    h(tail)->next = node;
                }

                tail = node;
                h(node)->in_list = true;
                return iterator{node};
            }

            pointer pop_front() {
                prs::assert(h(head)->in_list);
                return erase(iterator{head});
            }

            pointer pop_back() {
                prs::assert(h(tail)->in_list);
                return erase(iterator{tail});
            }

            iterator insert(iterator before, pointer node) {
                if (!before.current) {
                    return push_back(node);
                } else if (before.current == head) {
                    return push_front(node);
                }

                prs::assert(node);
                prs::assert(!h(node)->in_list);
                prs::assert(!h(node)->next);
                prs::assert(!h(node)->previous);

                pointer prev = this->prev(before.current);

                h(node)->next = before.current;
                h(node)->previous = prev;
                h(prev)->next = node;
                h(before.current)->previous = node;

                h(node)->in_list = true;
                return iterator{node};
            }

            void splice(iterator pos, list &other) {
                prs::assert(!pos.current);

                if(!other.head)
                    return;

                prs::assert(h(other.head)->in_list);
                prs::assert(!h(other.head)->previous);
                if(!tail) {
                    head = other.head;
                } else {
                    h(other.head)->previous = tail;
                    h(tail)->next = other.head;
                }

                tail = other.tail;
                other.head = nullptr;
                other.tail = nullptr;
            }

            iterator erase(iterator pos) {
                prs::assert(pos.current);
                prs::assert(h(pos.current)->in_list);
                pointer next = this->next(pos.current);
                pointer previous = this->prev(pos.current);

                if(!next) {
                    prs::assert(tail == pos.current);
                    tail = previous;
                } else {
                    prs::assert(h(next)->previous == pos.current);
                    h(next)->previous = previous;
                }

                pointer erased;
                if(!previous) {
                    prs::assert(head == pos.current);
                    erased = head;
                    head = next;
                } else {
                    prs::assert(h(previous)->next == pos.current);
                    erased = this->next(previous);
                    h(previous)->next = next;
                }

                prs::assert(erased == pos.current);
                h(pos.current)->next = nullptr;
                h(pos.current)->previous = nullptr;
                h(pos.current)->in_list = false;
                return iterator{erased};
            }

            iterator begin() {
                return iterator{head};
            }

            iterator end() {
                return iterator{nullptr};
            }

            reverse_iterator rbegin() {
                return reverse_iterator{tail};
            }

            reverse_iterator rend() {
                return reverse_iterator{nullptr};
            }

            pointer next(pointer before) {
                return static_cast<pointer>(h(before)->next);
            }

            pointer prev(pointer after) {
                return static_cast<pointer>(h(after)->previous);
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