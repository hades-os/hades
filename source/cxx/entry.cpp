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
#include <sys/sched.hpp>
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

static void taskA() {
    while (true) {
        kmsg("Hello from Task A");
    }
}

static void taskB() {
    while (true) {

    }
}

static void kern_task() {
    auto taskAT = sched::create_thread(taskA, (uint64_t) memory::pmm::stack(2), memory::vmm::common::boot_ctx, 0);

    auto taskBTCtx = memory::vmm::create();
    memory::vmm::map(0, (void *) memory::common::kernelBase, 8, VMM_PRESENT | VMM_LARGE | VMM_USER, taskBTCtx);
    
    if (memory::pmm::nr_pages * memory::common::page_size < VMM_4GIB) {
        memory::vmm::map(0, (void *) memory::common::virtualBase, VMM_4GIB / memory::common::page_size_2MB, VMM_PRESENT | VMM_LARGE | VMM_WRITE | VMM_USER, taskBTCtx);
    } else {
        memory::vmm::map(0, (void *) memory::common::virtualBase, ((memory::pmm::nr_pages * memory::common::page_size) / memory::common::page_size_2MB), VMM_PRESENT | VMM_LARGE | VMM_WRITE | VMM_USER, taskBTCtx);
    }

    auto taskBT = sched::create_thread(taskB, (uint64_t) memory::pmm::stack(2), (memory::vmm::vmm_ctx *) taskBTCtx, 3);

    sched::start_thread(taskBT);

    while (true) {
        asm volatile("hlt");
    }
}

extern "C" {
    [[noreturn]]
    void arch_entry(stivale::boot::header *header) {
        initarray_run();
        stivale::parser = {header};

        kern >> log::loggers::qemu;
        log::loggers::vesa::init(stivale::parser.fb());
        kern >> log::loggers::vesa::log;
        kern >> log::loggers::serial;

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

        vfs::mgr->mount("/dev/sda1", "/", vfs::fslist::FAT, nullptr, vfs::mflags::OVERLAY);

        auto file = vfs::mgr->open("/EFI/BOOT/BOOTX64.EFI", 0);
        kmsg("BOOTX64.EFI FD: ", file);

        auto buf = kmalloc(256);
        auto res = vfs::mgr->read(file, buf, 256);

        kmsg("READ BOOTX64.EFI: ", res);

        sched::init();

        auto kern_thread = sched::create_thread(kern_task, (uint64_t) memory::pmm::stack(2), memory::vmm::common::boot_ctx, 0);
        sched::start_thread(kern_thread);
        
        irq::on();
        
        while (true) {
            asm volatile("pause");
        }
    }
}