#ifndef TIME_HPP
#define TIME_HPP

#include <cstddef>
#include <cstdint>

namespace sched {    
    constexpr size_t TIMER_HZ = 1000000000;
    struct timespec {
        public:
            int64_t tv_sec;
            long tv_nsec;

            timespec operator+(timespec const& other) {
                timespec res = {
                    .tv_sec = this->tv_sec + other.tv_sec,
                    .tv_nsec = this->tv_nsec + other.tv_nsec
                };

                if (res.tv_nsec > TIMER_HZ) {
                    res.tv_nsec -= TIMER_HZ;
                    res.tv_sec++;
                }

                return res;
            };

            timespec operator-(timespec const &other) {
                timespec res = {
                    .tv_sec = this->tv_sec - other.tv_sec,
                    .tv_nsec = this->tv_nsec - other.tv_nsec
                };

                if (res.tv_nsec < 0) {
                    res.tv_nsec += TIMER_HZ;
                    res.tv_sec--;
                }

                if (res.tv_sec < 0) {
                    res.tv_nsec = 0;
                    res.tv_sec = 0;
                }

                return res;                    
            }

            timespec ms(int ms);
    };

    inline timespec clock_rt{};
    inline timespec clock_mono{};
}

#endif