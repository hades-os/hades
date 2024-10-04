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

                inline size_t col;
                inline size_t nr_cols;

                constexpr uint32_t rgb(size_t r, size_t g, size_t b) {
                    return (r << 16) | (g << 8) | b;
                }

                constexpr auto bg = rgb(65, 74, 76);
                constexpr auto fg = rgb(255, 250, 250);

                constexpr auto fontHeight = 16;
                constexpr auto fontWidth = 8;
                constexpr auto fontSize = fontWidth * fontHeight;

                inline void draw(size_t x, size_t y, uint32_t color) {
                    ((uint32_t *) address)[(y * width) + x] = color;
                    //*(volatile uint32_t*) (((uint64_t) address + memory::common::virtualBase) + ((y * pitch) + (x * bpp / 8))) = color;
                }

                inline void draw(size_t x, size_t y, char c) {
                    uint16_t offset = ((uint8_t)c - 0x20) * fontHeight;
                    for(uint8_t i = 0, i_cnt = fontWidth; i < fontWidth && i_cnt > 0; i++, i_cnt--) {
                        for(uint8_t j = 0; j < fontHeight; j++) {
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

                col = 0;
                nr_cols = width / fontWidth;
                memset((((uint32_t *) address) + (height - fontHeight) * width), bg, fontSize * width);
            }

            inline void log(const char *arg) {
                while (*arg) {
                    char c = *arg;
                    if (c == '\n' || col > nr_cols) {
                        col = 0;

                        for (size_t y = 1; y < height / fontHeight; y++) {
                            memcpy((((uint32_t *) address) + (y - 1) * fontHeight * width), (((uint32_t *) address) + y * fontHeight * width), fontSize * width);
                        }
                        memset((((uint32_t *) address) + (height - fontHeight) * width), bg, fontSize * width);
                    } else {
                        draw(col * fontWidth, height - fontHeight, (char) c);
                    }

                    col++;
                    arg++;
                }
            }
        };
    };
}

#endif