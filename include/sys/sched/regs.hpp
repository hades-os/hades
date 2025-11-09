#ifndef REGS_HPP
#define REGS_HPP

#include <sys/irq.hpp>
#include <cstdint>

namespace sched {
    struct regs {
        uint64_t rax, rbx, rcx, rdx, rbp, rdi, rsi, r8, r9, r10, r11, r12, r13, r14, r15;
        uint64_t rsp, rip;
        
        uint64_t ss, cs, fs;
        uint64_t rflags;
        uint64_t cr3;

        uint32_t mxcsr;
        uint16_t fcw;
    };

    inline irq::regs to_irq(sched::regs *regs) {
        return {
            .r15 = regs->r15,
            .r14 = regs->r14,
            .r13 = regs->r13,
            .r12 = regs->r12,
            .r11 = regs->r11,
            .r10 = regs->r10,
            .r9 = regs->r9,
            .r8 = regs->r8,
            .rsi = regs->rsi,
            .rdi = regs->rdi,
            .rbp = regs->rbx,
            .rdx = regs->rdx,
            .rcx = regs->rcx,
            .rbx = regs->rbx,
            .rax = regs->rax,

            .int_no = 0,
            .err = 0,

            .rip = regs->rip,
            .cs = regs->cs,
            .rflags = regs->rflags,
            .rsp = regs->rsp,
            .ss = regs->ss,
        };
    }
}

#endif