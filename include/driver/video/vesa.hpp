#ifndef VESA_LOG_HPP
#define VESA_LOG_HPP

#include <cstddef>
#include <mm/common.hpp>
#include <util/stivale.hpp>
#include <util/string.hpp>

namespace video {
    namespace vesa {
        void init(stivale::boot::tags::framebuffer fbinfo);
        void log(const char *arg);
        void display_bmp(void *buf, size_t size);
    };
}

#endif