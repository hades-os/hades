#ifndef PIT_HPP
#define PIT_HPP

#include <cstddef>
#include <mm/mm.hpp>

namespace pit {
    constexpr size_t PIT_FREQ = 100;

    void init();
}

#endif