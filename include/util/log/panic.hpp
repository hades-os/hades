#include "sys/irq.hpp"
#include "sys/smp.hpp"
#include "util/log/log.hpp"

inline void send_panic_ipis() {
    auto info = smp::get_locals();
    for (auto cpu : smp::cpus) {
        if (info->lid == cpu->lid) {
            continue;
        }
        
        apic::lapic::ipi(cpu->lid, 251);
    }
}

template<typename... Args>
[[noreturn]]
void panic(const Args& ...args) {
    irq::off();
    send_panic_ipis();
    ((util::kern << "[P] ") << ... << args) << util::endl;

    while (true) {
        asm volatile("pause");
    }
}