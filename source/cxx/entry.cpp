#include "mm/common.hpp"
#include "sys/sched/mail.hpp"
#include "sys/sched/signal.hpp"
#include <cstddef>
#include <cstdint>
#include <driver/ahci.hpp>
#include <frg/allocation.hpp>
#include <frg/string.hpp>
#include <fs/vfs.hpp>
#include <fs/dev.hpp>
#include <fs/fat.hpp>
#include <mm/mm.hpp>
#include <mm/pmm.hpp>
#include <mm/vmm.hpp>
#include <sys/acpi.hpp>
#include <sys/x86/apic.hpp>
#include <sys/pci.hpp>
#include <sys/smp.hpp>
#include <sys/irq.hpp>
#include <sys/sched/sched.hpp>
#include <sys/pit.hpp>
#include <util/stivale.hpp>
#include <util/log/qemu.hpp>
#include <util/log/serial.hpp>
#include <util/log/vesa.hpp>
#include <util/log/log.hpp>
#include <util/string.hpp>

extern "C" {
	extern void *_init_array_begin;
	extern void *_init_array_end;
}

void initarray_run() {
	uintptr_t start = (uintptr_t) &_init_array_begin;
	uintptr_t end = (uintptr_t) &_init_array_end;

	for (uintptr_t ptr = start; ptr < end; ptr += 8) {
		auto fn = (void(*) ())(*(uintptr_t *) ptr);
		fn();
	}
}

static void kern_task() {
    auto ctx = (memory::vmm::vmm_ctx *) memory::vmm::create();
    auto stack = (uint64_t) memory::vmm::map(nullptr, 4 * memory::common::page_size, VMM_PRESENT | VMM_USER | VMM_WRITE | VMM_MANAGED, (void *) ctx) + (4 * memory::common::page_size);
    auto proc = sched::create_process("init", 0, stack, ctx, 3);
    char *argv[] = { "/bin/init", NULL };
    char *forkv[] = { "/bin/child", NULL };
    char *envp[] = { "HOME=/", "PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin", "TERM=linux", "FBDEV=/dev/fb0", NULL };

    memory::vmm::change(proc->mem_ctx);
    proc->env.load_elf("/sys/bin/init.elf");
    proc->main_thread->reg.rip = proc->env.entry;

    proc->env.place_params(envp, argv, proc->main_thread);
    auto forked = sched::fork(proc, proc->main_thread);
    auto other_forked = sched::fork(proc, proc->main_thread);

    sched::signal::sigset_t newset = ~SIGMASK(SIGCHLD);
    sched::signal::do_sigprocmask(proc, SIG_SETMASK, &newset, nullptr);

    proc->start();
    forked->start();
    other_forked->start();

    sched::exec(forked->main_thread, "/sys/bin/child.elf", forkv, envp);

    sched::signal::send_process(nullptr, other_forked, SIGKILL);


    for (;;) {
        auto [exit_val, pid] = proc->waitpid(-1, proc->main_thread, 0);
        kmsg("Pid: ", pid, ", reaped, exit status: ", exit_val);
    }

    while (true) {
        asm volatile("hlt");
    }
}

extern "C" {
    [[noreturn]]
    void arch_entry(stivale::boot::header *header) {
        initarray_run();
        stivale::parser = {header};

        util::kern >> log::loggers::qemu;
        log::loggers::vesa::init(stivale::parser.fb());
        util::kern >> log::loggers::vesa::log;
        util::kern >> log::loggers::serial;

        kmsg("Booted by ", header->brand, " version ", header->version);

        acpi::init(stivale::parser.rsdp());

        irq::setup();
        irq::hook();

        memory::pmm::init(stivale::parser.mmap());
        memory::vmm::init();

        smp::init();
        smp::tss::init();
        
        acpi::madt::init();
        apic::init();
        
        pci::init();

        vfs::init();
        vfs::devfs::init();
        ahci::init();

        vfs::mount("/dev/sda1", "/", vfs::fslist::FAT, nullptr, vfs::mflags::OVERLAY);
        sched::init();
        pit::init();

        auto kern_thread = sched::create_thread(kern_task, (uint64_t) memory::pmm::stack(2), memory::vmm::common::boot_ctx, 0);
        kern_thread->start();
        
        irq::on();
        
        while (true) {
            asm volatile("pause");
        }
    }
}