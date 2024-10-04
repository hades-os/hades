#include "mm/common.hpp"
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
sched::regs default_user_regs{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x23, 0x1B, 0, 0x202, 0 };

util::lock sched::sched_lock{};

static void _idle() {
    while (1) { asm volatile("hlt"); };
}

static void _tick_handler(irq::regs *regs) {
    sched::tick_bsp(regs);
}

static void tick_ap(irq::regs *r) {
    sched::swap_task(r);
}

void sched::tick_bsp(irq::regs *r) {
    send_ipis();
    swap_task(r);
}

void sched::sleep(size_t time) {
    volatile uint64_t final_time = uptime + (time * (PIT_FREQ / 1000));

    final_time += 1;
    while (uptime < final_time) {  }
}

void sched::init() {
    irq::add_handler(&_tick_handler, 34);
    irq::add_handler(&tick_ap, 253);

    uint16_t x = 1193182 / PIT_FREQ;
    if ((1193182 % PIT_FREQ) > (PIT_FREQ / 2)) {
        x++;
    }

    io::ports::write<uint8_t>(0x43, 0x36);
    io::ports::write<uint8_t>(0x40, (uint8_t)(x & 0x00FF));
    io::ports::io_wait();
    io::ports::write<uint8_t>(0x40, (uint8_t)(x & 0xFF00) >> 8);
    io::ports::io_wait();
    apic::gsi_mask(apic::get_gsi(0), 0);

    init_locals();
}

void sched::init_locals() {
    uint64_t idle_rsp = (uint64_t) memory::pmm::stack(1);
    
    auto idle_task = create_thread(_idle, idle_rsp, memory::vmm::common::boot_ctx, 0);
    idle_task->state = thread::BLOCKED;
    auto idle_tid = start_thread(idle_task);

    smp::get_locals()->idle_tid = idle_tid;
    smp::get_locals()->tid = idle_tid;
}

void sched::send_ipis() {
    auto info = smp::get_locals();
    for (auto cpu : smp::cpus) {
        if (info->lid == cpu->lid) {
            continue;
        }
        
        apic::lapic::ipi(cpu->lid, (1 << 14) | 253);
    }
}

sched::thread *sched::create_thread(void (*main)(), uint64_t rsp, memory::vmm::vmm_ctx *ctx, uint8_t privilege) {
    thread *task = frg::construct<thread>(memory::mm::heap);
    
    task->kstack = (size_t) memory::pmm::stack(4);
    task->ustack = rsp;
    if (privilege == 3) {
        task->reg = default_user_regs;
    } else {
        task->reg = default_kernel_regs;
    }

    task->reg.rip = (uint64_t) main;
    task->ustack = rsp;
    task->reg.rsp = rsp;
    task->privilege = privilege;
    task->mem_ctx = ctx;

    task->reg.cr3 = (uint64_t) memory::vmm::cr3(ctx);
    task->state = thread::READY;

    task->env.envc = 0;
    task->env.argc = 0;
    task->env.auxc = 0;
    task->env.env = nullptr;
    task->env.auxvs = nullptr;
    task->env.argv = nullptr;
    task->parent = nullptr;

    return task;
}

void sched::kill_thread(int64_t tid) {
    sched_lock.acquire();

    auto thread = threads[tid];
    thread->state = thread::BLOCKED;

    if (thread->cpu != -1) {
        auto cpu = thread->cpu;
        sched_lock.release();

        apic::lapic::ipi(cpu, (1 << 14) | 253);
        while (thread->cpu != -1) { asm("pause"); };

        sched_lock.acquire();
    }

    threads[tid] = (sched::thread *) 0;
    frg::destruct(memory::mm::heap, thread);

    sched_lock.release();
}

int64_t sched::start_thread(thread *task) {
    sched_lock.irq_acquire();

    int64_t tid;
    threads.push_back(task);
    tid = threads.size() - 1;
    task->tid = tid;

    sched_lock.irq_release();

    return tid;
}

int64_t sched::pick_task() {
    for (int64_t t = smp::get_tid() + 1; (uint64_t) t < threads.size(); t++) {
        auto task = threads[t];
        if (task) {
            if (task->state == thread::READY || task->state == thread::WAIT) {
                return task->tid;
            }
        }
    }

    for (int64_t t = 0; t < smp::get_tid() + 1; t++) {
        auto task = threads[t];
        if (task) {
            if (task->state == thread::READY || task->state == thread::WAIT) {
                return task->tid;
            }
        }
    }

    return -1;;
}

void sched::swap_task(irq::regs *r) {
    sched_lock.acquire();

    auto running_task = smp::get_thread();
    if (running_task) {
        running_task->reg.rax = r->rax;
        running_task->reg.rbx = r->rbx;
        running_task->reg.rcx = r->rcx;
        running_task->reg.rdx = r->rdx;
        running_task->reg.rbp = r->rbp;
        running_task->reg.rdi = r->rdi;
        running_task->reg.rsi = r->rsi;
        running_task->reg.r8 = r->r8;
        running_task->reg.r9 = r->r9;
        running_task->reg.r10 = r->r10;
        running_task->reg.r11 = r->r11;
        running_task->reg.r12 = r->r12;
        running_task->reg.r13 = r->r13;
        running_task->reg.r14 = r->r14;
        running_task->reg.r15 = r->r15;

        running_task->reg.rflags = r->rflags;
        running_task->reg.rip = r->rip;
        running_task->reg.rsp = r->rsp;

        running_task->reg.cs = r->cs;
        running_task->reg.ss = r->ss;

        running_task->kstack = smp::get_locals()->tss.rsp0;
        running_task->ustack = smp::get_locals()->ustack;

        running_task->stopped = io::tsc();
        running_task->uptime += running_task->stopped - running_task->started;
        running_task->cpu = -1;

        running_task->reg.cr3 = memory::vmm::read_cr3();

        if (running_task->running) {
            running_task->running = false;
        }

        if (running_task->state == thread::RUNNING && running_task->tid != smp::get_locals()->idle_tid) {
            running_task->state = thread::READY;
        }
    }

    int64_t next_tid = pick_task();
    if (next_tid == -1) {
        smp::get_locals()->task = threads[smp::get_locals()->idle_tid];
        smp::get_locals()->tid =  smp::get_locals()->idle_tid;
        running_task = smp::get_thread();   
    } else {
        auto next_task = threads[next_tid];
        smp::get_locals()->task = next_task;
        smp::get_locals()->tid = next_tid;
        running_task = smp::get_thread();

        running_task->running = true;
        running_task->state = thread::RUNNING;
    }

    running_task->cpu = smp::get_locals()->lid;

    r->rax = running_task->reg.rax;
    r->rbx = running_task->reg.rbx;
    r->rcx = running_task->reg.rcx;
    r->rdx = running_task->reg.rdx;
    r->rbp = running_task->reg.rbp;
    r->rdi = running_task->reg.rdi;
    r->rsi = running_task->reg.rsi;
    r->r8 = running_task->reg.r8;
    r->r9 = running_task->reg.r9;
    r->r10 = running_task->reg.r10;
    r->r11 = running_task->reg.r11;
    r->r12 = running_task->reg.r12;
    r->r13 = running_task->reg.r13;
    r->r14 = running_task->reg.r14;
    r->r15 = running_task->reg.r15;

    r->rflags = running_task->reg.rflags;
    r->rip = running_task->reg.rip;
    r->rsp = running_task->reg.rsp;

    r->cs = running_task->reg.cs;
    r->ss = running_task->reg.ss;

    io::wrmsr(smp::fsBase, running_task->reg.fs);

    smp::get_locals()->kstack = running_task->kstack;
    smp::get_locals()->tss.rsp0 = running_task->kstack;
    smp::get_locals()->ustack = running_task->ustack;

    running_task->started = io::tsc();

    if (memory::vmm::read_cr3() != running_task->reg.cr3) {
        memory::vmm::write_cr3(running_task->reg.cr3);
    }

    sched_lock.release();
}