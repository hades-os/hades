#ifndef REGS_HPP
#define REGS_HPP

#include <cstdint>
namespace sched {
    struct regs {
        uint64_t rax, rbx, rcx, rdx, rbp, rdi, rsi, r8, r9, r10, r11, r12, r13, r14, r15;
        uint64_t rsp, rip;
        
        uint64_t ss, cs, fs;
        uint64_t rflags;
        uint64_t cr3;
    };
}

#endif