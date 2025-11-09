#ifndef QEMU_HPP
#define QEMU_HPP

#include <cstdint>
#include <util/io.hpp>

namespace log {
    namespace loggers {
        constexpr uint16_t qemuPort = 0xE9;
        inline void qemu(const char *arg) {
            if (io::ports::read<uint8_t>(qemuPort) != qemuPort) {
                return;
            }

            char *str = (char *) arg;
            while (*str) {
                io::ports::write(qemuPort, *str);
                str++;
            }
        };
    };
}

#endif