#include "mm/mm.hpp"
#include <cstddef>
#include <util/io.hpp>
#include <sys/pit.hpp>
#include <sys/irq.hpp>
#include <sys/smp.hpp>
#include <sys/x86/apic.hpp>
#include <sys/sched/time.hpp>
#include <sys/sched/sched.hpp>

void tick_handler(irq::regs *r) {
    sched::timespec interval = { .tv_sec = 0, .tv_nsec = pit::TIMER_HZ / sched::PIT_FREQ };
    sched::uptime += interval.tv_nsec;

    sched::clock_rt = sched::clock_rt + interval;
    sched::clock_mono = sched::clock_mono + interval;

    for (size_t i = 0; i < pit::timers.size(); i++) {
        auto timer = pit::timers[i];
        if (timer == nullptr) continue;

        timer->spec = timer->spec - interval;
        if (timer->spec.tv_nsec == 0 && timer->spec.tv_sec == 0) {
            for (size_t j = 0; j < timer->triggers.size(); j++) {
                auto trigger = timer->triggers[i];
                trigger->arise(smp::get_thread());
                frg::destruct(memory::mm::heap, trigger);
            }

            timer->triggers.~vector();
            pit::timers[i] = nullptr;
        }
    }

    sched::tick_bsp(r);
}

void pit::init() {
    irq::add_handler(tick_handler, irq::IRQ0);

    uint16_t divisor = 1193182 / PIT_FREQ;
    if ((1193182 % PIT_FREQ) > (PIT_FREQ / 2)) {
        divisor++;
    }

    io::ports::write<uint8_t>(0x43, (0b010 << 1) | (0b11 << 4));
    io::ports::write<uint8_t>(0x40, (uint8_t)(divisor & 0xFF));
    io::ports::write<uint8_t>(0x40, (uint8_t)(divisor >> 8 & 0xFF));

    apic::ioapic::route(0, 0, irq::IRQ0, false);

    sched::clock_mono = { .tv_sec = 0, .tv_nsec = 0 };
    sched::clock_rt = { .tv_sec = 0, .tv_nsec = 0 };
}