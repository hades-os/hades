#include "arch/x86/types.hpp"
#include <sys/x86/apic.hpp>
#include <util/io.hpp>
#include <arch/types.hpp>

uint8_t io::readb(uint16_t port) {
    auto ret = 0;
    asm volatile ("inb %%dx, %%al": "=a" (ret): "d" (port));
    return ret;
}

void io::writeb(uint16_t port, uint8_t val) {
    asm volatile ("outb %%al, %%dx":: "d" (port), "a" (val));
}

uint16_t io::readw(uint16_t port) {
    auto ret = 0;
    asm volatile ("inw %%dx, %%ax": "=a" (ret): "d" (port));
    return ret;
}

void io::writew(uint16_t port, uint16_t val) {
    asm volatile ("outw %%ax, %%dx":: "d" (port), "a" (val));
}

uint32_t io::readd(uint16_t port) {
    auto ret = 0;
    asm volatile ("inl %% dx, %% eax": "=a" (ret): "d" (port));
    return ret;
}

void io::writed(uint16_t port, uint32_t val) {
    asm volatile ("outl %% eax, %% dx":: "d" (port), "a" (val));
}

void io::wait() {
    io::writeb(0x80, 0);
}

void arch::init_features() {
    apic::init();
}

arch::irq_regs arch::sched_to_irq(arch::sched_regs *regs) {
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
        .ss = regs->ss
    };
}