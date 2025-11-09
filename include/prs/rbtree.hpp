#ifndef PRS_RBTREE_HPP
#define PRS_RBTREE_HPP

#include <utility>
#include "prs/assert.hpp"
namespace prs {
    struct null_aggregator {
        template<typename T>
        static bool aggregate(T *node) {
            (void)node;
            return false;
        }

        template<typename S, typename T>
        static bool check_invariant(S &tree, T *node) {
            (void)tree;
            (void)node;
            return true;
        }
    };

    enum class rbtree_color {
        none, red, black
    };

    struct rbtree_hook {
        public:
            rbtree_hook():
                    parent(nullptr),
                    left(nullptr), right(nullptr),
                    predecessor(nullptr), successor(nullptr), color(rbtree_color::none) {}

            rbtree_hook(const rbtree_hook &other) = delete;
            rbtree_hook &operator= (const rbtree_hook &other) = delete;

            void *parent;
            void *left;
            void *right;
            void *predecessor;
            void *successor;
            rbtree_color color;
    };

    template<typename T, rbtree_hook T:: *Member, typename L, typename A = null_aggregator>
    struct rbtree {
        private:
            T *_root;
            L _less;

            static rbtree_hook *h(T *item) {
                return &(item->*Member);
            }

            static bool is_red(T *node) {
                if(!node)
                    return false;

                return h(node)->color == rbtree_color::red;
            }

            static bool is_black(T *node) {
                if(!node)
                    return true;

                return h(node)->color == rbtree_color::black;
            }

            void aggregate_node(T *node) {
                A::aggregate(node);
            }

            void aggregate_path(T *node) {
                T *current = node;
                while(current) {
                    if(!A::aggregate(current))
                        break;
                    current = parent(current);
                }
            }

            void rotate_left(T *n) {
                T *u = parent(n);
                FRG_ASSERT(u != nullptr && right(u) == n);
                T *v = left(n);
                T *w = parent(u);

                if(v != nullptr)
                    h(v)->parent = u;
                h(u)->right = v;
                h(u)->parent = n;
                h(n)->left = u;
                h(n)->parent = w;

                if(w == nullptr) {
                    _root = n;
                } else if (left(w) == u) {
                    h(w)->left = n;
                } else {
                    prs::assert(right(w) == u);
                    h(w)->right = n;
                }

                aggregate_node(u);
                aggregate_node(n);
            }

            void rotate_right(T *n) {
                T *u = parent(n);
                prs::assert(u != nullptr && left(u) == n);
                T *v = right(n);
                T *w = parent(u);

                if(v != nullptr)
                    h(v)->parent = u;
                h(u)->left = v;
                h(u)->parent = n;
                h(n)->right = u;
                h(n)->parent = w;

                if(w == nullptr) {
                    _root = n;
                } else if (left(w) == u) {
                    h(w)->left = n;
                } else {
                    prs::assert(right(w) == u);
                    h(w)->right = n;
                }

                aggregate_node(u);
                aggregate_node(n);
            }

            void insert_fix(T *node) {
                T *parent = this->parent(node);
                if (parent == nullptr) {
                    h(node)->color = rbtree_color::black;
                    return;
                }

                h(node)->color = rbtree_color::red;
                if (h(parent)->color == rbtree_color::black)
                    return;

                T *grand = this->parent(parent);
                prs::assert(grand && h(grand)->color == rbtree_color::black);

                if (left(grand) == parent && is_red(right(grand))) {
                    h(grand)->color = rbtree_color::red;
                    h(parent)->color = rbtree_color::black;
                    h(right(grand))->color = rbtree_color::black;

                    insert_fix(grand);
                    return;
                } else if (right(grand) == parent && is_red(left(grand))) {
                    h(grand)->color = rbtree_color::red;
                    h(parent)->color = rbtree_color::black;
                    h(left(grand))->color = rbtree_color::black;

                    insert_fix(grand);
                    return;
                }

                if (parent == left(grand)) {
                    if (node == right(parent)) {
                        rotate_left(node);
                        rotate_right(node);

                        h(node)->color = rbtree_color::black;
                    } else {
                        rotate_right(parent);
                        h(parent)->color = rbtree_color::black;
                    }

                    h(grand)->color = rbtree_color::red;
                } else {
                    prs::assert(parent == right(grand));
                    if (node == left(parent)) {
                        rotate_right(node);
                        rotate_left(node);
                        h(node)->color = rbtree_color::black;
                    } else {
                        rotate_left(parent);
                        h(parent)->color = rbtree_color::black;
                    }

                    h(grand)->color = rbtree_color::red;
                }
            }

            void replace(T *node, T *replacement) {
                T *parent = this->parent(node);
                T *left = this->left(node);
                T *right = this->right(node);

                if (parent == nullptr) {
                    _root = replacement;
                } else if (node == this->left(parent)) {
                    h(parent)->left = replacement;
                } else {
                    h(parent)->right = replacement;
                }

                h(replacement)->parent = parent;
                h(replacement)->color = h(node)->color;
                
                h(replacement)->left = left;
                if (left)
                    h(left)->parent = replacement;

                h(replacement)->right = right;
                if (right)
                    h(right)->parent = replacement;

                if (predecessor(node))
                    h(predecessor(node))->successor = replacement;

                h(replacement)->predecessor = predecessor(node);
                h(replacement)->successor = successor(node);

                if (successor(node))
                    h(successor(node))->predecessor = replacement;
                
                h(node)->left = nullptr;
                h(node)->right = nullptr;
                h(node)->parent = nullptr;
                h(node)->predecessor = nullptr;
                h(node)->successor = nullptr;

                aggregate_node(replacement);
                aggregate_path(parent);                
            }

            void remove_fix(T *node) {
                prs::assert(h(node)->color == rbtree_color::black);

                T *parent = this->parent(node);
                if (parent == nullptr)
                    return;

                T *s;
                if (left(parent) == node) {
                    prs::assert(right(parent));
                    if (h(right(parent))->color == rbtree_color::red) {
                        T *x = right(parent);
                        rotate_left(right(parent));
                        prs::assert(node == left(parent));

                        h(parent)->color = rbtree_color::red;
                        h(x)->color = rbtree_color::black;
                    }

                    s = right(parent);
                } else {
                    prs::assert(right(parent) == node);
                    prs::assert(left(parent));

                    if (h(left(parent))->color == rbtree_color::red) {
                        T *x = left(parent);
                        rotate_right(x);
                        prs::assert(node == right(parent));

                        h(parent)->color = rbtree_color::red;
                        h(x)->color = rbtree_color::black;
                    }

                    s = left(parent);
                }

                if (is_black(left(s)) && is_black(right(s))) {
                    if (h(parent)->color == rbtree_color::black) {
                        h(s)->color = rbtree_color::red;
                        remove_fix(parent);
                        return;
                    } else {
                        h(parent)->color = rbtree_color::black;
                        h(s)->color = rbtree_color::red;
                        return;
                    }
                }

                auto parent_color = h(parent)->color;
                if (left(parent) == node) {
                    if (is_red(left(s)) && is_black(right(s))) {
                        T *child = left(s);
                        rotate_right(child);

                        h(s)->color = rbtree_color::red;
                        h(child)->color = rbtree_color::black;

                        s = child;
                    }

                    prs::assert(is_red(right(s)));

                    rotate_left(s);
                    h(parent)->color = rbtree_color::black;
                    h(s)->color = parent_color;
                    h(right(s))->color = rbtree_color::black;
                } else {
                    prs::assert(right(parent) == node);
                    if (is_red(right(s)) && is_black(left(s))) {
                        T *child = right(s);
                        rotate_left(child);

                        h(s)->color = rbtree_color::red;
                        h(child)->color = rbtree_color::black;

                        s = child;
                    }

                    prs::assert(is_red(left(s)));
                    
                    rotate_right(s);
                    h(parent)->color = rbtree_color::black;
                    h(s)->color = parent_color;
                    h(left(s))->color = rbtree_color::black;
                }
            }

            void remove_half_leaf(T *node, T *child) {
                T *pred = predecessor(node);
                T *succ = successor(node);

                if (pred)
                    h(pred)->successor = succ;
                if (succ)
                    h(succ)->predecessor = pred;

                if (h(node)->color == rbtree_color::black) {
                    if (is_red(child)) {
                        h(child)->color = rbtree_color::black;
                    } else {
                        remove_fix(node);
                    }
                }

                prs::assert((!left(node) && right(node) == child)
                    || (left(node) == child && !right(node)));

                T *parent = this->parent(node);
                if (!parent) {
                    _root = child;
                } else if (left(parent) == node) {
                    h(parent)->left = child;
                } else {
                    prs::assert(right(parent) == node);
                    h(parent)->right = child;
                }

                if (child)
                    h(child)->parent = parent;

                h(node)->left = nullptr;
                h(node)->right = nullptr;
                h(node)->parent = nullptr;
                h(node)->predecessor = nullptr;
                h(node)->successor = nullptr;                  
                
                if (parent)
                    aggregate_path(parent);
            }
        public:
            rbtree(L less = L())
                : _root{nullptr}, _less(std::move(less)) { }

            rbtree(const rbtree &other) = delete;
            rbtree &operator= (const rbtree &other) = delete;

            static T *parent(T *item) {
                return static_cast<T *>(h(item)->parent);
            }

            static T *left(T *item) {
                return static_cast<T *>(h(item)->left);
            }

            static T *right(T *item) {
                return static_cast<T *>(h(item)->right);
            }

            static T *predecessor(T *item) {
                return static_cast<T *>(h(item)->predecessor);
            }

            static T *successor(T *item) {
                return static_cast<T *>(h(item)->successor);
            }

            T *first() {
                T *current = root();
                if (!current)
                    return nullptr;
                while(left(current))
                    current = left(current);
                return current;
            }

            T *last() {
                T *current = root();
                if (!current)
                    return nullptr;
                while(right(current))
                    current = right(current);
                return current;
            }

            T *root() {
                return static_cast<T *>(_root);
            }

            // insertions
            void insert_root(T *node) {
                prs::assert(!_root);
                _root = node;

                aggregate_node(node);
                insert_fix(node);
            }

            void insert_left(T *parent, T *node) {
                prs::assert(parent);
                prs::assert(!left(parent));

                h(parent)->left = node;
                h(node)->parent = parent;

                T *pred = predecessor(parent);
                if (pred)
                    h(pred)->successor = node;
                h(node)->predecessor = pred;
                h(node)->successor = parent;
                h(parent)->predecessor = node;

                aggregate_node(node);
                aggregate_path(parent);
                insert_fix(node);
            }

            void insert_right(T *parent, T *node) {
                prs::assert(parent);
                prs::assert(!right(parent));

                h(parent)->right = node;
                h(node)->parent = parent;

                T *succ = successor(parent);
                h(parent)->successor = node;
                h(node)->predecessor = parent;
                h(node)->successor = succ;
                if(succ)
                    h(succ)->predecessor = node;

                aggregate_node(node);
                aggregate_path(parent);
                insert_fix(node);
            }

            void insert(T *node) {
                if (!root()) {
                    insert_root(node);
                    return;
                }

                T *current = root();
                while(true) {
                    if(_less(*node, *current)) {
                        if(left(current) == nullptr) {
                            insert_left(current, node);
                            return;
                        } else {
                            current = left(current);
                        }
                    } else {
                        if(right(current) == nullptr) {
                            insert_right(current, node);
                            return;
                        } else {
                            current = right(current);
                        }
                    }
                }                
            }

            void remove(T *node) {
                T *left = this->left(node);
                T *right = this->right(node);

                if (!left) {
                    remove_half_leaf(node, right);
                } else if (!right) {
                    remove_half_leaf(node, left);
                } else {
                    T *pred = predecessor(node);
                    remove_half_leaf(pred, this->left(pred));
                    replace(node, pred);
                }
            }
    };
}

#endif