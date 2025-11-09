#ifndef PRS_STRING_HPP
#define PRS_STRING_HPP

#include "mm/slab.hpp"
#include "prs/allocator.hpp"
#include "prs/basic_string.hpp"
 
namespace prs {
    struct default_string_allocator: prs::allocator {
        public:
           default_string_allocator():
                prs::allocator{slab::create_resource()} {}
    };

    using string = basic_string<char, default_string_allocator>;
    using string_view = basic_string_view<char>;
}

#endif 