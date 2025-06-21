#include "acpispec/resources.h"
#include "arch/types.hpp"
#include "driver/bus/pci.hpp"
#include "driver/dtable.hpp"
#include "driver/net/e1000.hpp"
#include "lai/helpers/pci.h"
#include "mm/common.hpp"
#include "mm/mm.hpp"
#include <cstddef>
#include <fs/dev.hpp>
#include <driver/pci/net.hpp>

vfs::devfs::device
    *pci::net::matcher::match(vfs::devfs::busdev *bus, void *aux) {
    auto pci_device = (pci::device *) aux;
    switch (pci_device->get_vendor()) {
        case intel_id: {
            switch(pci_device->get_device()) {
                case emu_id:
                case i217_id:
                case lm_id: {
                    pci::bar pci_bar;
                    if (!pci_device->read_bar(0, pci_bar)) {
                        break;
                    }

                    bool is_e1000e = pci_device->get_device() == i217_id || pci_device->get_device() == lm_id;
                    uint8_t is_mmio = pci_bar.is_mmio;
                    uint16_t io_base = pci_bar.base;
                    uint64_t mem_base = pci_bar.base;

                    pci_device->enable_busmastering();
                    if (is_mmio) pci_device->enable_mmio();

                    size_t vector = arch::alloc_vector();

                    // bar = prs::construct<pci_space>(memory::mm::heap, pci_bar.base, pci_bar.size, pci_bar.is_mmio)
                    ::net::setup_args args {
                        .flash_space = 0,
                        .reg_space = prs::construct<pci_space>(prs::allocator{slab::create_resource()}, is_mmio ? mem_base : io_base, 128 * memory::page_size, is_mmio),
                        .irq = (int) vector,
                        .flags = is_e1000e
                    };

                    auto e1000_dev = prs::construct<e1000::device>(prs::allocator{slab::create_resource()}, bus, dtable::majors::NET, -1, &args);
                    bool success = e1000_dev->setup();
                    if (!success) {
                        prs::destruct(prs::allocator{slab::create_resource()}, e1000_dev);
                        return nullptr;
                    }

                    return e1000_dev;
                }
            };

            break;
        }
    }

    return nullptr;
}

void pci::net::matcher::attach(vfs::devfs::busdev *bus, vfs::devfs::device *dev, void *aux) {
    auto pci_device = (pci::device *) aux;
    switch (dev->major) {
        case dtable::majors::NET: {
            auto e1000_dev = (e1000::device *) dev;

            acpi_resource_t irq_resource;
            auto err = lai_pci_route_pin(&irq_resource, 0, pci_device->get_bus(), pci_device->get_slot(), pci_device->get_func(), pci_device->read_pin());
            if (err > 0) {
                break;
            }

            arch::install_vector(e1000_dev->get_vector(), e1000::irq_handler, e1000_dev);
            arch::route_irq(irq_resource.base, e1000_dev->get_vector());

            e1000_dev->init_routing();
        }
    }
}