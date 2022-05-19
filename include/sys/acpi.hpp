#ifndef ACPI_HPP
#define ACPI_HPP

#include <cstddef>
#include <cstdint>
#include <frg/vector.hpp>
#include <mm/mm.hpp>
#include <util/stivale.hpp>

namespace acpi {
    struct [[gnu::packed]] sdt {
        char signature[4];
        uint32_t length;
        uint8_t version;
        uint8_t chk;
        char oemid[6];
        char oemtableid[8];
        uint32_t oemversion;
        uint32_t creatorid;
        uint32_t creatorversion;
    };

    namespace madt {
        struct [[gnu::packed]] ioapic {
            uint8_t type;
            uint8_t length;
            uint8_t id;
            uint8_t reserved;
            uint32_t address;
            uint32_t gsi_base;
        };

        struct [[gnu::packed]] iso {
            uint8_t type;
            uint8_t length;
            uint8_t bus;
            uint8_t irq;
            uint32_t gsi;
            uint16_t flags;
        };

        struct [[gnu::packed]] lapic {
            uint8_t type;
            uint8_t length;
            uint8_t pid;
            uint8_t id;
            uint32_t flags;
        };

        struct [[gnu::packed]] nmi {
            uint8_t type;
            uint8_t length;
            uint8_t pid;
            uint16_t flags;
            uint8_t lint;
        };

        struct [[gnu::packed]] header {
            acpi::sdt table;
            uint32_t lapic_address;
            uint32_t flags;
        };

        extern frg::vector<ioapic *, memory::mm::heap_allocator> ioapics;
        extern frg::vector<iso *, memory::mm::heap_allocator> isos;
        extern frg::vector<nmi *, memory::mm::heap_allocator> nmis;

        namespace {
            inline madt::header *_madt{};
        }

        extern void init();
    };

    namespace {
        struct [[gnu::packed]] rsdt {
            sdt _sdt;
            uint32_t ptrs[];
        };

        struct [[gnu::packed]] xsdt {
            sdt _sdt;
            uint64_t ptrs[];
        };

        struct [[gnu::packed]] rsdp {
            char signature[8];
            uint8_t chk;
            char oemid[6];
            uint8_t version;
            uint32_t rsdt;

            uint32_t length;
            uint64_t xsdt;
            uint8_t xchk;
            uint8_t reserved[3];
        };

        extern acpi::sdt *tables[22];

        extern acpi::xsdt *_xsdt;
        extern acpi::rsdt *_rsdt;

        extern acpi::rsdp *_rsdp;

        extern uint8_t use_xsdt;

        extern void _locate(const char *sig);
        extern uint8_t _rsdp_check();
        extern uint8_t _rsdt_check();
        extern uint8_t _xsdt_check();
    }

    void init(stivale::boot::tags::rsdp *info);
    acpi::sdt *table(const char *sig);
};

#endif