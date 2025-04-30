#ifndef ACPI_HPP
#define ACPI_HPP

#include <cstddef>
#include <cstdint>
#include <frg/vector.hpp>
#include <mm/mm.hpp>
#include <util/stivale.hpp>
#include "mm/arena.hpp"

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

    struct [[gnu::packed]] gas {
        uint8_t address_space;
        uint8_t bit_width;
        uint8_t bit_off;
        uint8_t access_size;
        uint64_t address;
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

        extern frg::vector<ioapic *, arena::allocator> ioapics;
        extern frg::vector<iso *, arena::allocator> isos;
        extern frg::vector<nmi *, arena::allocator> nmis;

        inline madt::header *_madt{};
        extern void init();
    };

    struct [[gnu::packed]] fadt {
        sdt _sdt;

        uint32_t fw_ctl;
        uint32_t dsdt;

        uint8_t rsv;

        uint8_t preferred_pwr;
        uint16_t sci_irq;
        uint32_t smi_cmd;

        uint8_t acpi_enable;
        uint8_t acpi_disable;

        uint8_t s4bios_req;
        uint8_t pstate_ctrl;

        uint32_t pm1a_evt_blk;
        uint32_t pm1b_evt_blk;

        uint32_t pm1a_ctrl_blk;
        uint32_t pm1b_ctrl_blk;

        uint32_t pm2_ctrl_blk;
        uint32_t pm2_timer_blk;

        uint32_t gpe0_block;
        uint32_t gpe1_block;

        uint8_t pm1_event_len;
        uint8_t pm1_ctrl_len;
        uint8_t pm2_ctrl_len;
        uint8_t pm_timer_len;

        uint8_t gpe0_length;
        uint8_t gpe1_length;
        uint8_t gpe1_base;

        uint8_t c_state_ctrl;
        
        uint16_t worst_c2_latency;
        uint16_t worst_c3_latency;

        uint16_t flush_size;
        uint16_t flush_stride;

        uint8_t duty_off;
        uint8_t duty_width;

        uint8_t day_alarm;
        uint8_t month_alarm;
        uint8_t century;

        uint16_t boot_arch_flags;
        uint8_t rsv2;
        uint32_t flags;

        gas reset_reg;
        uint8_t reset_val;
        uint8_t rsv3[3];

        uint64_t x_fw_ctl;
        uint64_t x_dsdt;
    };

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

    uint8_t _rsdp_check();
    uint8_t _rsdt_check();
    uint8_t _xsdt_check();

    void init(stivale::boot::tags::rsdp *info);
    acpi::sdt *table(const char *sig, size_t index);
};

#endif