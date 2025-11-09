#include <cstddef>
#include <cstdint>
#include <frg/allocation.hpp>
#include <mm/common.hpp>
#include <mm/pmm.hpp>
#include <mm/vmm.hpp>
#include <mm/mm.hpp>
#include <sys/acpi.hpp>
#include <sys/irq.hpp>
#include <sys/smp.hpp>
#include <sys/x86/apic.hpp>
#include <sys/sched/sched.hpp>
#include <util/io.hpp>
#include <util/lock.hpp>
#include <util/log/log.hpp>
#include <util/stivale.hpp>
#include <util/string.hpp>

[[noreturn]]
static inline void processorPanic(irq::regs *_) {
    irq::off();
    while (true) {
        asm volatile("pause");
    }
}

static inline util::lock cpuBootupLock{};
extern "C" {
    void processorEntry(stivale::boot::info::processor *_) {
        cpuBootupLock.acquire();
        
        auto *cpu = (smp::processor *) _->extra_argument;
        io::wrmsr(smp::gsBase, cpu);

        apic::lapic::setup();
        irq::hook();
        irq::on();

        kmsg("[CPU", cpu->lid, "] ", "kstack: ", util::hex(cpu->kstack));

        cpuBootupLock.release();

        memory::vmm::change(cpu->ctx);
        sched::init_locals();
        smp::tss::init();

        while (true) {
            asm volatile("pause");
        }
    }
};

extern "C" {
    extern void smp64_start(stivale::boot::info::processor *_);
};

void smp::init() {
    irq::add_handler(&processorPanic, 251);

    auto procs = stivale::parser.smp();
    for (auto stivale_cpu = procs->begin(); stivale_cpu != procs->end(); stivale_cpu++) {
        size_t lapic_id = stivale_cpu->lapic_id;
        auto processor = frg::construct<smp::processor>(memory::mm::heap, lapic_id);
        processor->kstack = (size_t) memory::pmm::stack(smp::initialStackSize);
        processor->ctx = memory::vmm::boot();
 
        if (!lapic_id) {
            io::wrmsr(smp::gsBase, processor);
            cpus.push_back(processor);
            continue;
        }

        stivale_cpu->extra_argument = (size_t) processor;
        stivale_cpu->target_stack = processor->kstack;
        stivale_cpu->goto_address = (size_t) &smp64_start;
    }

    cpuBootupLock.await();
}

smp::processor *smp::get_locals() {
    return io::rdmsr<smp::processor *>(smp::gsBase);
}

static inline smp::tss::gdtr real_gdt;
static inline util::lock tssLock{};

void smp::tss::init() {
    tssLock.acquire();
    asm volatile("sgdt (%0)" : : "r"(&real_gdt));

    auto *cpuInfo = get_locals();
    auto *tss = &cpuInfo->tss;
    uint64_t tss_ptr = (uint64_t) tss;
    descriptor *desc = (descriptor *) (real_gdt.ptr + TSS_OFFSET);
    
    memset(desc, 0, sizeof(descriptor));

    desc->base_lo = (uint32_t) ((tss_ptr >> 0) & 0xFFFF);
    desc->base_mid = (uint32_t) ((tss_ptr >> 16) & 0xFF);
    desc->base_mid2 = (uint32_t) ((tss_ptr >> 24) & 0xFF);
    desc->base_hi = (uint32_t) ((tss_ptr >> 32) & 0xFFFFFFFF);

    desc->limit_lo = 0x68;
    desc->pr = 1;
    desc->type = 0b1001;

    tss->ist[0] = (uint64_t) memory::pmm::stack(4);
    tss->ist[1] = (uint64_t) memory::pmm::stack(4);
    tss->rsp0 = tss->ist[0];

    kmsg("TSS: ", util::hex(tss->ist[0]));

    asm volatile("ltr %%ax" :: "a"(TSS_OFFSET));
    tssLock.release();
}

sched::thread *smp::get_thread() {
    return get_locals()->task;
}

sched::process *smp::get_process() {
    return get_locals()->proc;
}

size_t smp::get_pid() {
    return get_locals()->proc->pid;
}

int64_t smp::get_tid() {
    return get_locals()->tid;
}