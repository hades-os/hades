#include <cstddef>
#include <frg/allocation.hpp>
#include <mm/common.hpp>
#include <mm/pmm.hpp>
#include <mm/vmm.hpp>
#include <mm/mm.hpp>
#include <sys/acpi.hpp>
#include <sys/irq.hpp>
#include <sys/smp.hpp>
#include <sys/x86/apic.hpp>
#include <util/io.hpp>
#include <util/lock.hpp>
#include <util/log/log.hpp>
#include <util/stivale.hpp>
#include <util/string.hpp>

[[noreturn]]
static inline void processorPanic(irq::regs *_) {
    irq::off();
    auto cpuInfo = io::rdmsr<smp::processor *>(smp::fsBase);
    kmsg("[SMP] CPU ", cpuInfo->lid, ": PANIC");
    while (true) {
        asm volatile("pause");
    }
}

static inline util::lock cpuBootupLock{};
extern "C" {
    void processorEntry(stivale::boot::info::processor *_) {
        cpuBootupLock.acquire();
        
        auto *cpu = (smp::processor *) _->extra_argument;
        io::wrmsr(smp::fsBase, cpu);

        apic::lapic::setup();
        irq::hook();
        irq::on();

        kmsg("[CPU", cpu->lid, "] ", "kstack: ", util::hex(cpu->kstack));

        cpuBootupLock.release();

        memory::vmm::change(cpu->ctx);
        smp::tss::init();

        while (true) {
            asm volatile("pause");
        }
    }
};

extern "C" {
    extern void smp64_start(stivale::boot::info::processor *_);
};

void initProcessor(smp::processor *cpu) {
    if (!cpu->lid) {
        io::wrmsr(smp::fsBase, cpu);
        return;
    }

    auto *proc = stivale::parser.smp()->get_cpu(cpu->lid);
    auto stack = (size_t) memory::pmm::stack(smp::initialStackSize);
    cpu->kstack = stack;
    cpu->ctx = memory::vmm::boot();

    proc->extra_argument = (size_t) cpu;
    proc->target_stack = stack;
    proc->goto_address = (size_t) &smp64_start;
}

void smp::init() {
    irq::add_handler(&processorPanic, 254);

    for (auto& stivale_cpu : *stivale::parser.smp()) {
        // because c++ is a shit language
        size_t lapic_id = stivale_cpu.lapic_id;
        auto processor = frg::construct<smp::processor>(memory::mm::heap, lapic_id);
        
        cpus.push_back(processor);
        initProcessor(processor);
    }

    cpuBootupLock.await();
}

static inline smp::tss::gdtr real_gdt;
static inline util::lock tssLock{};

void smp::tss::init() {
    tssLock.acquire();
    asm volatile("sgdt (%0)" : : "r"(&real_gdt));

    auto *cpuInfo = io::rdmsr<smp::processor *>(smp::fsBase);
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

    asm volatile("ltr %%ax" :: "a"(TSS_OFFSET));
    tssLock.release();
}