#ifndef PRS_ASSERT_HPP
#define PRS_ASSERT_HPP

#include <source_location>

namespace prs {
    void panic(const char *fmt, ...);
    inline void assert(bool cond, 
        std::source_location location = std::source_location::current()) {
        if (!cond)
            panic("%s: %s: Assertion failed!", location.file_name(), location.line());
    }
}

#endif