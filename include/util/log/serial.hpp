#ifndef SERIAL_HPP
#define SERIAL_HPP

#include <cstdint>
#include <util/io.hpp>

namespace log {
    namespace loggers {
        constexpr uint16_t serialPort = 0x3F8;
        static inline bool serialInitialized = false;
        namespace {
            static inline void init() {
                io::ports::write<uint8_t>(serialPort + 1, 0x00);    // Disable all interrupts
                io::ports::write<uint8_t>(serialPort + 3, 0x80);    // Enable DLAB (set baud rate divisor)
                io::ports::write<uint8_t>(serialPort + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
                io::ports::write<uint8_t>(serialPort + 1, 0x00);    // (hi byte)
                io::ports::write<uint8_t>(serialPort + 3, 0x03);    // 8 bits, no parity, one stop bit
                io::ports::write<uint8_t>(serialPort + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
                io::ports::write<uint8_t>(serialPort + 4, 0x0B);    // IRQs enabled, RTS/DSR set
                serialInitialized = true;
            }
        };

        inline void serial(const char *arg) {
            if (!serialInitialized) {
                init();            
            }

            while (!(io::ports::read<uint8_t>(serialPort + 5) & 0x20)) {
                asm volatile("pause");
            }

            char *str = (char *) arg;
            while (*str) {
                io::ports::write<uint8_t>(serialPort, *str);
                str++;
            }
        };
    };
}

#endif