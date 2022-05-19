#ifndef LOG_HPP
#define LOG_HPP

#include <cstddef>
#include <cstdint>
#include <util/lock.hpp>
#include <sys/smp.hpp>
#include <sys/irq.hpp>
#include <sys/x86/apic.hpp>
#include <util/io.hpp>

namespace util {
    static constexpr auto endl = "\n";
    static constexpr auto pfx = "[8086] ";

    template<typename T>
    static constexpr void *hex(T val) {
        return (void *) val;
    }

    template<typename T>
    static constexpr size_t dec(T val) {
        return (size_t) val;
    }
    
    class stream {
        public:
            typedef void (*logger)(const char *str);
        private:
            static constexpr auto log_size = 1000;
            static constexpr auto digits_upper = "0123456789ABCDEF";
            static constexpr auto digits_lower = "0123456789abcdef";
            static constexpr auto num_buf_len = 48;

            char num_buf[num_buf_len];
            logger loggers[32] = { 0 };

            char *num_fmt(char *buf, size_t buf_len, uint64_t i, int base, int padding, char pad_with, int handle_signed, int upper, int len) {
                int neg = (signed) i < 0 && handle_signed;

                if (neg)
                    i = (unsigned) (-((signed) i));

                char *ptr = buf + buf_len - 1;
                *ptr = '\0';

                const char *digits = upper ? digits_upper : digits_lower;

                do {
                    *--ptr = digits[i % base];
                    if (padding)
                        padding--;
                    if (len > 0)
                        len--;
                    buf_len--;
                } while ((i /= base) != 0 && (len == -1 || len) && buf_len);

                while (padding && buf_len) {
                    *--ptr = pad_with;
                    padding--;
                    buf_len--;
                }

                if (neg && buf_len)
                    *--ptr = '-';

                return ptr;
            }

        public:
            stream& operator <<(const char *arg) {
                for (int i = 0; i < 32; i++) {
                    if (loggers[i] != 0) {
                        loggers[i](arg);
                    }
                }
                return *this;
            };

            stream& operator <<(char *arg) {
                *this << (const char *) arg;
                return *this;
            };

            stream& operator <<(size_t arg) {
                auto fmt = num_fmt(num_buf, num_buf_len, (uint64_t) arg, 10, 0, ' ', ((int64_t) arg > 0) ? 0 : 1, 0, -1);
                *this << fmt;
                return *this;
            };

            stream& operator <<(void *arg) {
                *this << "0x";
                auto fmt = num_fmt(num_buf, num_buf_len, (uint64_t) arg, 16, 0, ' ', 0, 0, 16);
                *this << fmt;
                return *this;
            };

            stream& operator <<(bool arg) {
                *this << (arg ? "true" : "false");
                return *this;
            }

            template<typename T>
            stream& operator <<(T obj) {
                if constexpr(std::is_pointer_v<T>) {
                    *this << (void *) obj;
                    return *this;
                }
                
                if constexpr(std::is_integral_v<T>) {
                    *this << (size_t) obj;
                    return *this;
                }
 
                *this << (void *) &obj;
                return *this;
            };

            stream& operator >>(util::stream::logger logger) {
                for (int i = 0; i < 32; i++) {
                    if (loggers[i] == 0) {
                        loggers[i] = logger;
                        return *this;
                    }
                }
                return *this;
            } 
    };
};

// global kernel log
inline util::stream klog{};

namespace {
    inline util::lock lock{};
};

template<typename... Args>
[[noreturn]]
void panic(const Args& ...args) {
    lock.acquire();
    irq::off();
    ((klog << "[P] ") << ... << args) << util::endl;
    
    auto info = io::rdmsr<smp::processor *>(smp::fsBase);;
    for (auto cpu : smp::cpus) {
        if (info->lid == cpu->lid) {
            continue;
        }
        
        apic::lapic::ipi(cpu->lid, 254);
    }

    lock.release();

    while (true) {
        asm volatile("pause");
    }
}

template<typename... Args>
void kmsg(const Args& ...args) {
    lock.acquire();
    ((klog << "[K] " )<< ... << args) << util::endl;
    lock.release();
}

#endif