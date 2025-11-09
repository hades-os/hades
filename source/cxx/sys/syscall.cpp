#include "sys/smp.hpp"
#include "sys/syscall.hpp"
#include <cstdint>
#include <util/log/log.hpp>
#include <sys/irq.hpp>

#define LENGTHOF(x) (sizeof(x) / sizeof(x[0]))

void syscall_0(irq::regs *r) {

}

void syscall_1(irq::regs *r) {

}

static syscall::handler syscalls_list[] = {
    syscall_0,
    syscall_1
};

void syscall::register_handler(syscall::handler handler, int syscall) {
    syscalls_list[syscall] = handler;
}

extern "C" {
    void syscall_handler(irq::regs *r) {
        uint64_t syscall_num = r->rax;
        if (syscall_num >= LENGTHOF(syscalls_list)) {
            r->rax = -1;
            // TODO: errno
            return;
        }

        // TODO: signal queue

        if (syscalls_list[syscall_num] != nullptr) {
            syscalls_list[syscall_num](r);
        }

        if (r->rax != -1) {
            // TODO: errno
        }


    }
}