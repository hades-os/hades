#include <cstdint>
#include <mm/common.hpp>
#include <sys/acpi.hpp>
#include <util/string.hpp>
#include <util/log/log.hpp>
#include <util/log/panic.hpp>

#define member(structType, elementType, structPtr, memberName) \
  ((elementType*)(((char*)(structPtr)) + offsetof(structType, memberName)))

const char *table_list[] = {
    "APIC",
    "BERT",
    "CPEP",
    "DSDT",
    "ECDT",
    "EINJ",
    "ERST",
    "FACP",
    "FACS",
    "HEST",
    "MSCT",
    "MPST",
    "OEMx",
    "PMTT",
    "PSDT",
    "RASF",
    "RSDT",
    "SBST",
    "SLIT",
    "SRAT",
    "SSDT",
    "XSDT"
};

namespace acpi {
    namespace madt {
        frg::vector<ioapic *, memory::mm::heap_allocator> ioapics{};
        frg::vector<iso *, memory::mm::heap_allocator> isos{};
        frg::vector<nmi *, memory::mm::heap_allocator> nmis{};
    };

    acpi::sdt *tables[22];

    acpi::xsdt *_xsdt = nullptr;
    acpi::rsdt *_rsdt = nullptr;

    acpi::rsdp *_rsdp = nullptr;

    uint8_t use_xsdt = 0;

    uint8_t _rsdp_check() {
        uint8_t sum = 0;
        if (acpi::_rsdp->version == 0) {
            for (size_t i = 0; i < sizeof(acpi::rsdp) - 16; i++) {
                sum += ((uint8_t *) acpi::_rsdp)[i];
            }
        } else {
            for (size_t i = 0; i < sizeof(acpi::rsdp); i++) {
                sum += ((uint8_t *) acpi::_rsdp)[i];
            }
        }
        return sum;
    }

    uint8_t _rsdt_check() {
        uint8_t sum = 0;
        for (size_t i = 0; i < acpi::_rsdt->_sdt.length; i++) {
            sum += ((uint8_t *) acpi::_rsdt)[i];
        }
        return sum;
    }

    uint8_t _xsdt_check() {
        return 0;
        uint8_t sum = 0;
        for (size_t i = 0; i < acpi::_xsdt->_sdt.length; i++) {
            sum += ((uint8_t *) acpi::_xsdt)[i];
        }
        return sum;
    }

    void _locate(const char *sig) {
        acpi::sdt *ptr;

        if (acpi::use_xsdt) {
            for (size_t i = 0; i < (acpi::_xsdt->_sdt.length - sizeof(acpi::sdt)) / 8; i++) {
                uint64_t *nptrs = member(acpi::xsdt, uint64_t, acpi::_xsdt, ptrs);;
                ptr = (acpi::sdt *) nptrs[i];
                ptr = (acpi::sdt *) ((char *) ptr + memory::common::virtualBase);
                if (!strncmp(ptr->signature, sig, 4)) {
                    kmsg("[ACPI] Found table ", sig);
                    acpi::tables[i] = ptr;
                }
            }
        } else {
            for (size_t i = 0; i < (acpi::_rsdt->_sdt.length - sizeof(acpi::sdt)) / 4; i++) {
                uint32_t *nptrs = member(acpi::rsdt, uint32_t, acpi::_rsdt, ptrs);
                ptr = (acpi::sdt *) nptrs[i];
                ptr = (acpi::sdt *) ((char *) ptr + memory::common::virtualBase);
                if (!strncmp(ptr->signature, sig, 4)) {
                    kmsg("[ACPI] Found table ", sig);
                    acpi::tables[i] = ptr;
                }
            }
        }
    }
}

void acpi::init(stivale::boot::tags::rsdp *info) {
    acpi::_rsdp = (acpi::rsdp *) (info->rsdp + memory::common::virtualBase);
    if (!acpi::_rsdp) {
        panic("[ACPI] RSDP Not Found!");
    }

    if ((acpi::_rsdp_check() & 0xF) == 0) {
        kmsg("[ACPI] RSDP Checksum is ", acpi::_rsdp_check());
    } else {
        panic("[ACPI] Corrupted RSDP!");
    }

    kmsg("[ACPI] OEM ID ", acpi::_rsdp->oemid);
    kmsg("[ACPI] RSDT Address is ", util::hex(acpi::_rsdp->rsdt));
    kmsg("[ACPI] ACPI Version ", acpi::_rsdp->version);

    acpi::_rsdt = (acpi::rsdt *) acpi::_rsdp->rsdt;
    if (acpi::_rsdp->version >= 2) {
        kmsg("[ACPI] XSDT Address is ", util::hex(acpi::_rsdp->xsdt));
        kmsg("[ACPI] RSDP (ACPI V2) Checksum is ", acpi::_xsdt_check());
        if ((acpi::_xsdt_check() % 0x100) != 0) {
            panic("[ACPI] Corrupted XSDT!");
        }

        acpi::use_xsdt = 1;
        acpi::_xsdt = (acpi::xsdt *) _rsdp->xsdt;
    } else {
        if ((acpi::_rsdt_check() % 0x100) != 0) {
            panic("[ACPI] Corrupted RSDT! ", acpi::_rsdt_check());
        }
    }

    acpi::_rsdt = memory::common::offsetVirtual(acpi::_rsdt);
    acpi::_xsdt = memory::common::offsetVirtual(acpi::_xsdt);

    for (size_t i = 0; i < 22; i++) {
        acpi::_locate(table_list[i]);
    }
}

acpi::sdt *acpi::table(const char *sig) {
    for (size_t i = 0; i < 22; i++) {
        auto sdt = acpi::tables[i];
        if (!strncmp(sdt->signature, sig, 4)) {
            return sdt;
        }
    }

    return nullptr;
}

void acpi::madt::init() {
    _madt = (madt::header *) acpi::table("APIC");
    uint64_t table_size = _madt->table.length - sizeof(madt::header);
    uint64_t list = (uint64_t) _madt + sizeof(madt::header);
    uint64_t offset = 0;
    while ((list + offset) < (list + table_size)) {
        uint8_t *item = (uint8_t *) (list + offset);
        switch (item[0]) {
            case 0: {
                break;
            };

            case 1: {
                madt::ioapic *ioapic = (madt::ioapic *) item;
                kmsg("[MADT] Found IOAPIC ", ioapic->id);
                ioapics.push_back(ioapic);
                break;
            };

            case 2: {
                madt::iso *iso = (madt::iso *) item;
                kmsg("[MADT] Found ISO ", isos.size());
                isos.push_back(iso);
                break;
            };

            case 4: {
                madt::nmi *nmi = (madt::nmi *) item;
                kmsg("[MADT] Found NMI ", nmis.size());
                nmis.push_back(nmi);
                break;
            };

            default:
                kmsg("[MADT] Unrecognized type ", item[0]);
                break;
        } 

        offset = offset + item[1];
    }
}