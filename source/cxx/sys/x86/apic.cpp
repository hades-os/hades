#include <cstddef>
#include <cstdint>
#include <sys/acpi.hpp>
#include <sys/x86/apic.hpp>
#include <util/io.hpp>
#include <util/log/log.hpp>

namespace apic {
    acpi::madt::iso *get_iso(uint32_t gsi) {
        for (auto iso : acpi::madt::isos) {
            if (iso->gsi == gsi) {
                return iso;
            }
        }

        return nullptr;
    };

    namespace ioapic {
        uint32_t read(size_t ioapic, uint32_t reg) {
            if (ioapic > acpi::madt::ioapics.size()) {
                panic("[IOAPIC] Invalid IOAPIC access of ", ioapic);
            }

            volatile uint32_t *base = (volatile uint32_t *) (acpi::madt::ioapics[ioapic]->address + memory::common::virtualBase);
            *base = reg;
            return *(base + 4);
        }

        void write(size_t ioapic, uint32_t reg, uint32_t data) {
            if (ioapic > acpi::madt::ioapics.size()) {
                panic("[IOAPIC] Invalid IOAPIC access of ", ioapic);
            }

            volatile uint32_t *base = (volatile uint32_t *) (acpi::madt::ioapics[ioapic]->address + memory::common::virtualBase);
            *base = reg;
            *(base + 4) = data;
        }

        void setup() {
            uint8_t vector = 0x20;
            for (size_t i = 0; i < acpi::madt::ioapics.size() && vector < 0x2F; i++) {
                size_t pins = apic::max_redirs(i);
                for (size_t j = 0; j <= pins; j++, vector++) {
                    acpi::madt::iso *iso = apic::get_iso(vector);
                    if (!iso) {
                        route(i, vector, j, 0, 0, 1);
                    } else {
                        route(i, vector, j, iso->flags, 0, 1);
                    }
                }
            }
        };
    };

    void route(uint64_t num, uint8_t irq, uint32_t pin, uint16_t flags, uint8_t apic, uint8_t masked) {
        size_t ent = irq;

        if (flags & 2) {
            ent |= IOAPIC_REDIR_POLARITY;
        }

        if (flags & 8) {
            ent |= IOAPIC_REDIR_TRIGGER_MODE;
        }

        if (masked) {
            ent |= IOAPIC_REDIR_MASK;
        }

        ent |= ((size_t) apic) << 56;

        uint32_t reg = pin * 2 + 16;
        ioapic::write(num, reg + 0, (uint32_t) ent);
        ioapic::write(num, reg + 1, (uint32_t) (ent >> 32));
    }

    size_t max_redirs(size_t ioapic) {
        return (ioapic::read(ioapic, 1) >> 16) & 0xFF;
    }

    void remap() {
        uint8_t master_mask = io::ports::read<uint8_t>(0x21);
        uint8_t slave_mask  = io::ports::read<uint8_t>(0xA1);

        if (master_mask == 0xFF && slave_mask == 0xFF) {
            return;
        }

        io::ports::write<uint8_t>(0x20, 0x11);
        io::ports::io_wait();
        io::ports::write<uint8_t>(0xA0, 0x11);
        io::ports::io_wait();

        io::ports::write<uint8_t>(0x21, 0x20);
        io::ports::io_wait();
        io::ports::write<uint8_t>(0xA1, 0x40);
        io::ports::io_wait();

        io::ports::write<uint8_t>(0x21, 4);
        io::ports::io_wait();
        io::ports::write<uint8_t>(0xA1, 2);
        io::ports::io_wait();

        io::ports::write<uint8_t>(0x21, 1);
        io::ports::io_wait();
        io::ports::write<uint8_t>(0xA1, 1);
        io::ports::io_wait();

        io::ports::write<uint8_t>(0x21, master_mask);
        io::ports::io_wait();
        io::ports::write<uint8_t>(0xA1, slave_mask);
        io::ports::io_wait();
        io::ports::write<uint8_t>(0xA1, 0xFF);
        io::ports::write<uint8_t>(0x21, 0xFF);
    };

    size_t gsi_to_ioapic(uint32_t gsi) {
        for (uint64_t i = 0; i < acpi::madt::ioapics.size(); i++) {
            acpi::madt::ioapic* ioapic = acpi::madt::ioapics[i];
            uint64_t max_redirs = apic::max_redirs(i);
            if (gsi >= ioapic->gsi_base && gsi <= ioapic->gsi_base + max_redirs) {
                return i;
            }
        }

        return -1;
    };

    void mask_pin(size_t ioapic, uint32_t pin, int masked)  {
        uint32_t reg = pin * 2 + 16;
        uint64_t ent = 0;
        ent |= ioapic::read(ioapic, reg + 0);
        ent |= (uint64_t) ioapic::read(ioapic, reg + 1) << 32;

        if (masked) {
            ent |= (1 << 16);
        } else {
            ent &= ~(1 << 16);
        }

        ioapic::write(ioapic, reg + 0, (uint32_t) ent);
        ioapic::write(ioapic, reg + 1, (uint32_t) (ent >> 32));
    };

    namespace lapic {
        uint32_t read(uint32_t reg) {
            size_t base = (size_t) get_base() + memory::common::virtualBase;
            return *(volatile uint32_t *) (base + reg);
        }

        void write(uint32_t reg, uint32_t data) {
            size_t base = (size_t) get_base() + memory::common::virtualBase;
            *((volatile uint32_t *) (base + reg)) = data;
        }

        void *get_base() {
            return (void *) (io::rdmsr<uint64_t>(0x1B) & 0xfffff000);
        };

        void set_base(void *base) {
            uint32_t rdx = (uint64_t) base >> 32;
            uint32_t rax = ((uint64_t) base & ~0xFFF) | LAPIC_BASE_MSR_ENABLE;

            io::wrmsr(LAPIC_BASE_MSR, ((uint64_t) rdx) >> 32 | rax);
        }

        void setup() {
            if (!(io::rdmsr<size_t>(LAPIC_BASE_MSR) & LAPIC_BASE_MSR_ENABLE)) {
                lapic::set_base(lapic::get_base());
            }
            lapic::write(LAPIC_REG_SIVR, apic::lapic::read(LAPIC_REG_SIVR) | LAPIC_BASE_MSR_BSP);
        }

        void eoi() {
            lapic::write(LAPIC_REG_EOI, 0);
        }

        void ipi(uint32_t ap, uint32_t flags) {
            lapic::write(LAPIC_REG_ICR_HIGH, ap << 24);
            lapic::write(LAPIC_REG_ICR_LOW, flags);
        }
    };

    void gsi_mask(uint32_t gsi, uint8_t masked) {
        size_t ioapic = apic::gsi_to_ioapic(gsi);

        uint32_t pin = gsi - acpi::madt::ioapics[ioapic]->gsi_base;

        mask_pin(ioapic, pin, masked);
    };

    int64_t get_gsi(uint8_t irq) {
        for (size_t i = 0; i < acpi::madt::isos.size(); i++) {
            if (acpi::madt::isos[i]->bus == 0 && acpi::madt::isos[i]->irq == irq) {
                return acpi::madt::isos[i]->gsi;
            }
        }

        return -1;
    };

    void init() {
        remap();
        lapic::setup();
        ioapic::setup();
    }
};
