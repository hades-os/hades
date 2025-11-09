#include <cstddef>
#include <cstdint>
#include <frg/allocation.hpp>
#include <frg/vector.hpp>
#include <mm/mm.hpp>
#include <mm/pmm.hpp>
#include <mm/vmm.hpp>
#include <sys/irq.hpp>
#include <sys/sched.hpp>
#include <sys/smp.hpp>
#include <sys/x86/apic.hpp>
#include <util/io.hpp>
#include <util/log/log.hpp>
#include <util/lock.hpp>

sched::regs default_kernel_regs{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x10, 0x8, 0, 0x202, 0 };
sched::regs default_user_regs{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x1B, 0x23, 0, 0x202, 0 };

static void _tick_handler(irq::regs *regs) {
    sched::uptime++;
}

void sched::sleep(size_t time) {
    volatile uint64_t final_time = uptime + (time * (PIT_FREQ / 1000));

    final_time++;

    while (uptime < final_time) {  }
}

void sched::init() {
    irq::add_handler(&_tick_handler, 34);

    kmsg("[PIT] PIT Freq: ", PIT_FREQ);

    uint16_t x = 1193182 / PIT_FREQ;
    if ((1193182 % PIT_FREQ) > (PIT_FREQ / 2)) {
        x++;
    }

    io::ports::write<uint8_t>(0x40, (uint8_t)(x & 0x00FF));
    io::ports::io_wait();
    io::ports::write<uint8_t>(0x40, (uint8_t)(x & 0xFF00) >> 8);
    io::ports::io_wait();
    apic::gsi_mask(apic::get_gsi(0), 0);

    irq::on();
}