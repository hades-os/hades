#ifndef PRS_RBTREE_HPP
#define PRS_RBTREE_HPP

namespace prs {
    struct rbtree_hook {
        public:
            enum class color_type {
                none, red, black
            };

            rbtree_hook():
                    parent(nullptr), 
                    left(nullptr), right(nullptr),
                    predecessor(nullptr), successor(nullptr), color(color_type::none) {}

            rbtree_hook(const rbtree_hook &other) = delete;
            rbtree_hook &operator= (const rbtree_hook &other) = delete;

            void *parent;
            void *left;
            void *right;
            void *predecessor;
            void *successor;
            color_type color;
    };

    template<typename T, rbtree_hook T:: *Member>
    struct rbtree {

    };
}

#endif