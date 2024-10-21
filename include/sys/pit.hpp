#ifndef PIT_HPP
#define PIT_HPP

#include <cstddef>
#include <frg/vector.hpp>
#include <mm/mm.hpp>
#include <sys/sched/mail.hpp>

namespace pit {
    constexpr size_t TIMER_HZ = 1000000000;
    constexpr size_t PIT_FREQ = 1000;
    constexpr size_t TIMER_MSG = 0x23;

    inline util::lock timers_lock{};
    inline frg::vector<ipc::mailbox *, memory::mm::heap_allocator> timers{};
    void init();
}

#endif