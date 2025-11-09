#ifndef PORTS_HPP
#define PORTS_HPP

#include <cstdint>

namespace io {
    namespace ports {
        template<typename S>
        void write(uint16_t port, S val);

        template<typename S>
        S read(uint16_t port);

        template<>
        inline uint8_t read(uint16_t port) {
            auto ret = 0;
            asm volatile ("inb %%dx, %%al": "=a" (ret): "d" (port));
            return ret;
        }

        template<>
        inline void write(uint16_t port, uint8_t value) {
            asm volatile ("outb %%al, %%dx":: "d" (port), "a" (value));
        }

        template<>
        inline void write(uint16_t port, char val) {
            write<uint8_t>(port, val);
        }

        template<>
        inline uint16_t read(uint16_t port) {
            auto ret = 0;
            asm volatile ("inw %%dx, %%ax": "=a" (ret): "d" (port));
            return ret;
        }

        template<>
        inline void write(uint16_t port, uint16_t value) {
            asm volatile ("outw %%ax, %%dx":: "d" (port), "a" (value));
        }

        template<>
        inline uint32_t read(uint16_t port) {
            auto ret = 0;
            asm volatile ("inl %% dx, %% eax": "=a" (ret): "d" (port));
            return ret;
        }

        template<>
        inline void write(uint16_t port, uint32_t value) {
            asm volatile ("outl %% eax, %% dx":: "d" (port), "a" (value));
        }

        inline void io_wait() {
            write<uint8_t>(0x80, 0x0);
        }
    };

    template<typename V>
    void wrmsr(uint64_t msr, V value) {
        uint32_t low = ((uint64_t) value) & 0xFFFFFFFF;
        uint32_t high = ((uint64_t) value) >> 32;
        asm volatile (
            "wrmsr"
            :
            : "c"(msr), "a"(low), "d"(high)
        );
    }
    
    template<typename V>
    V rdmsr(uint64_t msr) {
        uint32_t low, high;
        asm volatile (
            "rdmsr"
            : "=a"(low), "=d"(high)
            : "c"(msr)
        );
        return (V) (((uint64_t ) high << 32) | low);
    }
};

#endif