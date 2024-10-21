#ifndef SYSCALL_HPP
#define SYSCALL_HPP

#include <sys/irq.hpp>
namespace syscall {
    using handler = void (*)(irq::regs *r);
    void register_handler(handler handler, int syscall);
}

#endif