#include <cstddef>
#include <driver/ahci.hpp>
#include <frg/allocation.hpp>
#include <frg/string.hpp>
#include <fs/vfs.hpp>
#include <fs/devfs.hpp>
#include <fs/initrd.hpp>
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

extern "C" {
    [[noreturn]]
    void arch_entry(stivale::boot::header *header) {
        initarray_run();
        stivale::parser = {header};

        klog >> log::loggers::qemu;
        log::loggers::vesa::init(stivale::parser.fb());
        klog >> log::loggers::vesa::log;
        klog >> log::loggers::serial;

        kmsg("Alea iacta est");
        kmsg("Booted by ", header->brand, " version ", header->version);

        acpi::init(stivale::parser.rsdp());

        irq::setup();
        irq::hook();

        memory::pmm::init(stivale::parser.mmap());
        memory::vmm::init();

        smp::init();
        acpi::madt::init();
        apic::init();
        smp::tss::init();

        pci::init();

        vfs::init();
        vfs::devfs::init();
        ahci::init();

        auto fd = vfs::mgr()->open("/dev/sda1", 0);
        auto buf = kmalloc(512);
        auto res = vfs::mgr()->read(fd, buf, 512);
        kmsg("result: ", res);
        
        while (true) {
            asm volatile("pause");
        }
    }
}