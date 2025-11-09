#include <arch/types.hpp>
#include <util/log/log.hpp>

template<typename... Args>
[[noreturn]]
void panic(const Args& ...args) {
    arch::irq_off();
    arch::stop_all_cpus();

    ((util::kern << "[P] ") << ... << args) << util::endl;
    while (true) {
        asm volatile("pause");
    }
}