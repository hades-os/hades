#ifndef BMP_HPP
#define BMP_HPP

#include <cstdint>
namespace video {
    namespace bmp {
        struct [[gnu::packed]] file_header {
            uint16_t type;
            uint32_t size;
            uint16_t rsvd0;
            uint16_t rsvd1;
            uint32_t pixel_off;
        };

        struct [[gnu::packed]] info_header {
            uint32_t header_size;
            int32_t width;
            int32_t height;
            uint16_t planes;
            uint16_t bpp;
            uint32_t compression;
            uint32_t image_size;
            uint32_t hres;
            uint32_t vres;
            uint32_t colors;
            uint32_t ign0;
        };
    };
}

#endif