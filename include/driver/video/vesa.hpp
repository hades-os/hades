#ifndef VESA_LOG_HPP
#define VESA_LOG_HPP

#include <cstddef>
#include <cstdint>
#include <mm/common.hpp>
#include <util/stivale.hpp>
#include <util/string.hpp>

#include "font-8x16.hpp"

namespace video {
    namespace vesa {
        void init(stivale::boot::tags::framebuffer fbinfo);
        void log(const char *arg);
        void display_bmp(void *buf, size_t size);
    };
}

#endif