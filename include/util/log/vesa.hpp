#ifndef VESA_LOG_HPP
#define VESA_LOG_HPP

#include <cstddef>
#include <cstdint>
#include <mm/common.hpp>
#include <util/font-8x16.hpp>
#include <util/stivale.hpp>
#include <util/string.hpp>

namespace log {
    namespace loggers {
        namespace vesa {
            namespace {
                inline size_t width;
                inline size_t height;
                inline size_t bpp;
                inline size_t pitch;
                inline size_t address;

                inline size_t row;
                inline size_t col;
                inline size_t nr_cols;
                inline size_t nr_rows;

                constexpr uint32_t rgb(size_t r, size_t g, size_t b) {
                    return (r << 16) | (g << 8) | b;
                }

                constexpr auto bg = rgb(16, 16, 16);
                constexpr auto fg = rgb(255, 250, 250);

                constexpr auto fontHeight = 16;
                constexpr auto fontWidth = 8;
                
                inline void draw(size_t x, size_t y, uint32_t color) {
                    ((uint32_t *) address)[(y * width) + x] = color;
                    //*(volatile uint32_t*) (((uint64_t) address + memory::common::virtualBase) + ((y * pitch) + (x * bpp / 8))) = color;
                }

                inline void draw(size_t x, size_t y, char c) {
                    uint16_t offset = ((uint8_t)c - 0x20) * 16;
                    for(uint8_t i = 0, i_cnt = 8; i < 8 && i_cnt > 0; i++, i_cnt--) {
                        for(uint8_t j = 0; j < 16; j++) {
                            if((util::font[offset + j] >> i) & 1) {
                                draw(x + i_cnt, y + j, fg);
                            }
                        }
                    }
                }
            };

            inline void init(stivale::boot::tags::framebuffer *fbinfo) {
                width = fbinfo->width;
                height = fbinfo->height;
                bpp = fbinfo->bpp;
                pitch = fbinfo->pitch;
                address = fbinfo->addr + memory::common::virtualBase;

                row = 0;
                col = 0;
                nr_cols = width / 8;
                nr_rows = height / 16;

                nr_rows--;

                for (size_t y = 0; y < height; y++) {
                    for (size_t x = 0; x < width; x++) {
                        draw(x, y, rgb(65, 74, 76));
                    }
                }
            }

            inline void log(const char *arg) {
                while (*arg) {
                    char c = *arg;
                    if (col > nr_cols) {
                        col = 0;
                        row++;
                    }

                    if (row > nr_rows) {
                        row = 0;
                    }

                    if (c == '\n') {
                        col = 0;
                        row++;
                    } else {
                        draw(col * 8, row * 16, (char) c);
                    }

                    col++;
                    arg++;
                }
            }
        };
    };
}

#endif