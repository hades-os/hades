#ifndef SCHED_HPP
#define SCHED_HPP

#include <cstddef>
#include <cstdint>
#include <frg/hash.hpp>
#include <frg/hash_map.hpp>
#include <frg/rbtree.hpp>
#include <frg/vector.hpp>
#include <mm/mm.hpp>
#include <sys/irq.hpp>
#include <util/lock.hpp>

namespace sched {
    struct regs {
        uint64_t rax, rbx, rcx, rdx, rbp, rdi, rsi, r8, r9, r10, r11, r12, r13, r14, r15;
        uint64_t rsp, rip;
        
        uint64_t ss, cs, gs;
        uint64_t rflags;
        uint64_t cr3;

        regs(uint64_t rax, uint64_t rbx, uint64_t rcx, uint64_t rdx, uint64_t rbp, uint64_t rdi, uint64_t rsi, uint64_t r8, uint64_t r9, uint64_t r10, uint64_t r11, uint64_t r12, uint64_t r13, uint64_t r14, uint64_t r15,
             uint64_t rsp, uint64_t rip,
             uint64_t ss, uint64_t cs, uint64_t fs, uint64_t rflags,
             uint64_t cr3) : rax(rax), rbx(rbx), rcx(rcx), rdx(rdx), rbp(rbp), rdi(rdi), rsi(rsi), r8(r8), r9(r9), r10(r10), r11(r11), r12(r12), r13(r13), r14(r14), r15(r15),
                             rsp(rsp), rip(rip),
                             ss(ss), cs(cs), gs(0), rflags(rflags),
                             cr3(cr3) 
            {}

        regs() : rax(0), rbx(0), rcx(0), rdx(0), rbp(0), rdi(0), rsi(0), r8(0), r9(0), r10(0), r11(0), r12(0), r13(0), r14(0), r15(0),
                             rsp(0), rip(0),
                             ss(0), cs(0), gs(0), rflags(0),
                             cr3(0) 
            {}

        regs(irq::regs *regs) {
            this->rax = regs->rax;
            this->rbx = regs->rbx;
            this->rcx = regs->rcx;
            this->rdx = regs->rdx;
            this->rbp = regs->rbp;
            this->rdi = regs->rdi;
            this->rsi = regs->rsi;
            this->r8  = regs->r8;
            this->r9  = regs->r9;
            this->r10 = regs->r10;
            this->r11 = regs->r11;
            this->r12 = regs->r12;
            this->r13 = regs->r13;
            this->r14 = regs->r14;
            this->r15 = regs->r15;

            this->rsp = regs->rsp;
            this->rip = regs->rip;

            this->rflags = regs->rflags;

            asm volatile("mov %%cr3, %0;" : "=r"(this->cr3));
        }
    };

    inline size_t uptime;

    constexpr size_t PIT_FREQ = 1000;
    void sleep(size_t time);

    void init();

};

#endif