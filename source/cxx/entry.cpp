#include "arch/types.hpp"
#include "arch/vmm.hpp"
#include "driver/keyboard.hpp"
#include "driver/net/e1000.hpp"
#include "driver/tty/tty.hpp"
#include "driver/tty/pty.hpp"
#include "driver/video/vt.hpp"
#include "fs/cache.hpp"
#include "lai/core.h"
#include "lai/helpers/sci.h"
#include "mm/arena.hpp"
#include "mm/common.hpp"
#include "mm/slab.hpp"
#include "sys/sched/signal.hpp"
#include "util/types.hpp"
#include <cstddef>
#include <cstdint>
#include <driver/ahci.hpp>
#include <frg/allocation.hpp>
#include <frg/string.hpp>
#include <fs/vfs.hpp>
#include <fs/dev.hpp>
#include <mm/mm.hpp>
#include <mm/pmm.hpp>
#include <mm/vmm.hpp>
#include <sys/acpi.hpp>
#include <sys/x86/apic.hpp>
#include <driver/bus/pci.hpp>
#include <sys/sched/sched.hpp>
#include <util/stivale.hpp>
#include <util/log/qemu.hpp>
#include <util/log/serial.hpp>
#include <driver/video/vesa.hpp>
#include <driver/video/fbdev.hpp>
#include <util/log/log.hpp>
#include <util/string.hpp>

static stivale::boot::tags::framebuffer fbinfo{};
static log::subsystem logger = log::make_subsystem("HADES");

static void run_init() {
    auto ctx = vmm::create();
    auto stack = ctx->stack(nullptr, memory::user_stack_size, vmm::map_flags::USER | vmm::map_flags::WRITE | vmm::map_flags::READ | vmm::map_flags::FILL_NOW);

    auto proc = sched::create_process((char *) "init", 0, (uint64_t) stack, ctx, 3);
    auto group = sched::create_process_group(proc);
    sched::create_session(proc, group);

    char *argv[] = { (char *)
        "/usr/bin/init", 
        NULL 
    };

    proc->cwd = vfs::resolve_at("/", nullptr);

    auto fd = vfs::open(nullptr, "/usr/bin/init", proc->fds, 0, O_RDWR, 0, 0);
    proc->mem_ctx->swap_in();
    
    proc->env.load_elf("/usr/bin/init", fd);
    proc->env.set_entry();

    proc->env.load_params(argv, nullptr);
    proc->env.place_params(nullptr, argv, proc->main_thread);

    kmsg(logger, "Trying to run init process, at: %lx", proc->env.entry);
    proc->start();
}

static void show_splash(shared_ptr<vfs::fd_table> table) {
    prs::allocator allocator{
        arena::create_resource()
    };

    auto splash_fd = vfs::open(nullptr, "/home/racemus/hades.bmp", table, 0, O_RDONLY, 0, 0);

    auto info = frg::construct<vfs::node::statinfo>(slab::create_resource());
    vfs::stat(nullptr, "/home/racemus/hades.bmp", info, 0);
     
    auto buffer = allocator.allocate(info->st_size);
    vfs::read(splash_fd, buffer, info->st_size);

    video::vesa::display_bmp(buffer, info->st_size);
}

static void kern_task() {
    lai_create_namespace();
    lai_enable_acpi(1);

    vfs::init();
    cache::init();
    
    vfs::devfs::init();
    vfs::devfs::probe();
    // TODO: fix device init

    vfs::mount("/dev/sdb", "/", vfs::fslist::EXT, 0);

    vt::init(fbinfo);
    tty::self::init();
    tty::ptmx::init();
    
    auto boot_table = vfs::make_table();
    tty::set_active("/dev/tty0", boot_table);
    fb::init(&fbinfo);

    kb::init();

    run_init();

    while (true) {
        asm volatile("hlt");
    }
}

extern "C" {
	extern void *__init_array_begin;
	extern void *__init_array_end;
}

using constructor_t = void();
static void run_constructors() {
	uintptr_t start = (uintptr_t) &__init_array_begin;
	uintptr_t end = (uintptr_t) &__init_array_end;

	for (uintptr_t ptr = start; ptr < end; ptr += 8) {
		auto fn = (constructor_t *)(*(uintptr_t *) ptr);
		fn();
	}
}

extern "C" {
    [[noreturn]]
    void arch_entry(stivale::boot::header *header) {
        run_constructors();

        stivale::parser = {header};

        fbinfo = *stivale::parser.fb();
        video::vesa::init(fbinfo);

        kmsg(logger, "Booted by %s, version %d", header->brand, header->version);

        acpi::init(stivale::parser.rsdp());
        arch::init_irqs();

        pmm::init(stivale::parser.mmap());
        vmm::init();

        acpi::madt::init();

        sched::init();

        auto kern_thread = sched::create_thread(kern_task, (uint64_t) pmm::stack(4), vmm::boot, 0);

        kern_thread->start();        
        while (true) {
            asm volatile("pause");
        }
    }
}