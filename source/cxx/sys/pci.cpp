#include <cstddef>
#include <cstdint>
#include <sys/pci.hpp>
#include <util/io.hpp>
#include <util/log/log.hpp>

uint8_t pci::device::get_bus() {
    return bus;
}

uint8_t pci::device::get_slot() {
    return slot;
}

uint8_t pci::device::get_clazz() {
    return clazz;
}

uint8_t pci::device::get_subclass() {
    return subclass;
}

uint8_t pci::device::get_prog_if() {
    return prog_if;
}

uint8_t pci::device::get_func() {
    return func;
}

uint16_t pci::device::get_device() {
    return devize;
}

uint16_t pci::device::get_vendor() {
    return vendor_id;
}

int64_t pci::device::get_parent() {
    return parent;
}

static inline uint32_t get_address(uint8_t bus, uint8_t slot, uint8_t func, uint8_t reg) {
    uint32_t lbus  = (uint32_t) bus;
    uint32_t lslot = (uint32_t) slot;
    uint32_t lfunc = (uint32_t) func;
 
    uint32_t address = (uint32_t) ((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (reg & 0xfc) | ((uint32_t) 0x80000000));
    return address;
}

static inline uint32_t read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t reg) {
    io::ports::write<uint32_t>(pci::CONFIG_PORT, get_address(bus, slot, func, reg));
    return io::ports::read<uint32_t>(pci::DATA_PORT + (reg & 3));
}

static inline void write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t reg, uint32_t data) {
    io::ports::write<uint32_t>(pci::CONFIG_PORT, get_address(bus, slot, func, reg));
    io::ports::write<uint32_t>(pci::DATA_PORT + (reg & 3), data);
}

static inline uint16_t read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t reg) {
    io::ports::write<uint32_t>(pci::CONFIG_PORT, get_address(bus, slot, func, reg));
    return io::ports::read<uint16_t>(pci::DATA_PORT + (reg & 3));
}

static inline void write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t reg, uint16_t data) {
    io::ports::write<uint32_t>(pci::CONFIG_PORT, get_address(bus, slot, func, reg));
    io::ports::write<uint16_t>(pci::DATA_PORT + (reg & 3), data);
}

static inline uint16_t read_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t reg) {
    io::ports::write<uint32_t>(pci::CONFIG_PORT, get_address(bus, slot, func, reg));
    return io::ports::read<uint8_t>(pci::DATA_PORT + (reg & 3));
}

static inline void write_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t reg, uint8_t data) {
    io::ports::write<uint32_t>(pci::CONFIG_PORT, get_address(bus, slot, func, reg));
    io::ports::write<uint8_t>(pci::DATA_PORT + (reg & 3), data);
}

static inline uint16_t get_vendor(uint8_t bus, uint8_t slot, uint8_t func) {
    return (uint16_t) read_dword(bus, slot, func, 0);
}

static inline uint16_t get_device(uint8_t bus, uint8_t slot, uint8_t func) {
    return (uint16_t) (read_dword(bus, slot, func, 0) >> 16);
}

static inline uint8_t get_class(uint8_t bus, uint8_t slot, uint8_t func) {
    uint8_t clazz = (uint8_t) (read_dword(bus, slot, func, 0x8) >> 24);
    return clazz;
}

static inline uint8_t get_subclass(uint8_t bus, uint8_t slot, uint8_t func) {
    uint8_t subclass = (uint8_t) (read_dword(bus, slot, func, 0x8) >> 16);
    return subclass;
}

static inline uint8_t get_prog_if(uint8_t bus, uint8_t slot, uint8_t func) {
    return (uint8_t) (read_dword(bus, slot, func, 0x8) >> 8);
}

static inline uint16_t get_status(uint8_t bus, uint8_t slot, uint8_t func) {
    return (uint16_t) (read_dword(bus, slot, func, 0x4) >> 16);
}

static inline uint8_t get_capability(uint8_t bus, uint8_t slot, uint8_t func, uint8_t capability) {
    uint16_t reg_status = get_status(bus, slot, func);
    uint8_t reg_cap = read_byte(bus, slot, func, 0x34);

    if (!(reg_status & (1 << 4))) {
        return 0;
    }

    uint16_t cap_word = read_word(bus, slot, func, reg_cap);

    uint8_t cap_id = (uint8_t) cap_word;
    uint8_t cap_next = (uint8_t) cap_word >> 8;

    cap_next &= 0xFC;

    while (cap_next) {
        if (cap_id == capability) {
            return 1;
        }

        cap_word = read_dword(bus, slot, func, cap_next);

        cap_id = (uint8_t) cap_word;
        cap_next = (uint8_t) cap_word >> 8;

        cap_next &= 0xFC;
    }

    return 0;
}

int pci::device::read_bar(size_t index, pci::bar& bar) {
    if (index > 5) {
        return 0;
    }

    uint64_t reg_idx = 0x10 + index * 4;
    uint64_t bar_lo = readd(reg_idx);
    uint64_t bar_size_low = 0;
    uint64_t bar_hi = 0;
    uint64_t bar_size_hi = 0;

    if (!bar_lo) {
        return 0;
    }

    uint64_t base;
    uint64_t size;

    uint8_t is_mmio = !(bar_lo & 1);
    uint8_t is_prefetchable = is_mmio && bar_lo & (1 << 3);
    uint8_t is_long = is_mmio && ((bar_lo >> 1) & 0x3) == 0x2;

    if (is_long) {
        bar_hi = readd(reg_idx + 4);
    }

    base = ((bar_hi << 32) | bar_lo) & ~(is_mmio ? (0xF) : (0x3));

    writed(reg_idx, 0xFFFFFFFF);
    bar_size_low = readd(reg_idx);
    writed(reg_idx, bar_lo);

    if (is_long) {
        writed(reg_idx + 4, 0xFFFFFFFF);
        bar_size_hi = readd(reg_idx + 4);
        writed(reg_idx + 4, bar_hi);
    }

    size = ((bar_size_hi << 32) | bar_size_low) & ~(is_mmio ? (0xF) : (0x3));
    size = ~size + 1;

    bar.base = base;
    bar.size = size;
    bar.is_mmio = is_mmio;
    bar.is_prefetchable = is_prefetchable;
    bar.valid = true;

    return 1;
}

int pci::device::register_msi(uint8_t vector, uint8_t lapic_id) {
    uint8_t off = 0;

    uint32_t config_4  = readd(pci::PCI_HAS_CAPS);
    uint8_t  config_34 = readb(pci::PCI_CAPS);

    if ((config_4 >> 16) & (1 << 4)) {
        uint8_t cap_off = config_34;

        while (cap_off) {
            uint8_t cap_id   = readb(cap_off);
            uint8_t cap_next = readb(cap_off + 1);

            switch (cap_id) {
                case 0x05: {
                    kmsg("[PCI]: Device has MSI support");
                    off = cap_off;
                    break;
                }
            }
            cap_off = cap_next;
        }
    }

    if (off == 0) {
        kmsg("[PCI]: Device does not support MSI");
        return 0;
    }

    uint16_t msi_opts = readw(off + MSI_OPT);
    if (msi_opts & MSI_64BIT_SUPPORTED) {
        msi_data data    = {.raw = 0};
        msi_address addr = {.raw = 0};
        addr.raw = readw(off + MSI_ADDR_LOW);
        data.raw = readw(off + MSI_DATA_64);
        data.vector = vector;

        data.delv_mode = 0;
        addr.base_addr = 0xFEE;
        addr.dest_id = lapic_id;
        writed(off + MSI_ADDR_LOW, addr.raw);
        writed(off + MSI_DATA_64, data.raw);
    } else {
        msi_data data    = {.raw = 0};
        msi_address addr = {.raw = 0};
        addr.raw = readw(off + MSI_ADDR_LOW);
        data.raw = readw(off + MSI_DATA_32);
        data.vector = vector;

        data.delv_mode = 0;
        addr.base_addr = 0xFEE;
        addr.dest_id = lapic_id;
        writed(off + MSI_ADDR_LOW, addr.raw);
        writed(off + MSI_DATA_32, data.raw);
    }

    msi_opts |= 1;
    writew(off + MSI_OPT, msi_opts);

    return 1;
}

uint8_t pci::device::readb(uint32_t offset) {
    return read_byte(bus, slot, func, offset);
}

void pci::device::writeb(uint32_t offset, uint8_t value) {
    write_byte(bus, slot, func, offset, value);
}

uint16_t pci::device::readw(uint32_t offset) {
    return read_word(bus, slot, func, offset);
}

void pci::device::writew(uint32_t offset, uint16_t value) {
    write_word(bus, slot, func, offset, value);
}

uint32_t pci::device::readd(uint32_t offset) {
    return read_dword(bus, slot, func, offset);
}

void pci::device::writed(uint32_t offset, uint32_t value) {
    write_dword(bus, slot, func, offset, value);
}

void pci::device::enable_busmastering() {
    if (!(readd(0x4) & (1 << 2))) {
        writed(0x4, readd(0x4) | (1 << 2));
    }
}

const char *pci::to_string(uint8_t clazz, uint8_t subclass, uint8_t prog_if) {
    switch (clazz) {
        case 0:
            return "Undefined";
        case 1:
            switch (subclass) {
                case 0:
                    return "SCSI Bus Controller";
                case 1:
                    return "IDE Controller";
                case 2:
                    return "Floppy Disk Controller";
                case 3:
                    return "IPI Bus Controller";
                case 4:
                    return "RAID Controller";
                case 5:
                    return "ATA Controller";
                case 6:
                    switch (prog_if) {
                        case 0:
                            return "Vendor Specific SATA Controller";
                        case 1:
                            return "AHCI SATA Controller";
                        case 2:
                            return "Serial Storage Bus SATA Controller";
                    }
                    break;
                case 7:
                    return "Serial Attached SCSI Controller";
                case 8:
                    switch (prog_if) {
                        case 1:
                            return "NVMHCI Controller";
                        case 2:
                            return "NVMe Controller";
                    }
                    break;
                return "Mass Storage Controller";
            }
        case 2:
            switch (subclass) {
                case 0:
                    return "Ethernet Controller";
                case 1:
                    return "Token Ring Controller";
                case 2:
                    return "FDDI Controller";
                case 3:
                    return "ATM Controller";
                case 4:
                    return "ISDN Controller";
                case 5:
                    return "WorldFip Controller";
                case 6:
                    return "PICMG 2.14 Controller";
                case 7:
                    return "InfiniBand Controller";
                case 8:
                    return "Fabric Controller";
            }
            return "Network Controller";
        case 3:
            switch (subclass) {
                case 0:
                    return "VGA Compatible Controller";
                case 1:
                    return "XGA Controller";
                case 2:
                    return "3D Controller";
            }
            return "Display Controller";
        case 4:
            return "Multimedia Controller";
        case 5:
            return "Memory Controller";
        case 6:
            switch (subclass) {
                case 0:
                    return "Host Bridge";
                case 1:
                    return "ISA Bridge";
                case 2:
                    return "EISA Bridge";
                case 3:
                    return "MCA Bridge";
                case 4:
                    return "PCI-to-PCI Bridge";
                case 5:
                    return "PCMCIA Bridge";
                case 6:
                    return "NuBus Bridge";
                case 7:
                    return "CardBus Bridge";
                case 8:
                    return "RACEway Bridge";
                case 9:
                    return "Semi-Transparent PCI-to-PCI Bridge";
                case 10:
                    return "InfiniBand-to-PCI Host Bridge";
            }
            return "Bridge Device";
        case 8:
            switch (subclass) {
                case 0:
                    switch (prog_if) {
                        case 0:
                            return "8259-Compatible PIC";
                        case 1:
                            return "ISA-Compatible PIC";
                        case 2:
                            return "EISA-Compatible PIC";
                        case 16:
                            return "I/O APIC IRQ Controller";
                        case 32:
                            return "I/O xAPIC IRQ Controller";
                    }
                    break;
                case 1:
                    switch (prog_if) {
                        case 0:
                            return "8239-Compatible DMA Controller";
                        case 1:
                            return "ISA-Compatible DMA Controller";
                        case 2:
                            return "EISA-Compatible DMA Controller";
                    }
                    break;
                case 2:
                    switch (prog_if) {
                        case 0:
                            return "8254-Compatible PIT";
                        case 1:
                            return "ISA-Compatible PIT";
                        case 2:
                            return "EISA-Compatible PIT";
                        case 3:
                            return "HPET";
                    }
                    break;
                case 3:
                    return "Real Time Clock";
                case 4:
                    return "PCI Hot-Plug Controller";
                case 5:
                    return "SDHCI";
                case 6:
                    return "IOMMU";
            }
            return "Base System Peripheral";
        case 12:
            switch (subclass) {
                case 0:
                    switch (prog_if) {
                        case 0:
                            return "Generic FireWire (IEEE 1394) Controller";
                        case 1:
                            return "OHCI FireWire (IEEE 1394) Controller";
                    }
                    break;
                case 1:
                    return "ACCESS Bus Controller";
                case 2:
                    return "SSA Controller";
                case 3:
                    switch (prog_if) {
                        case 0:
                            return "UHCI USB1 Controller";
                        case 16:
                            return "OHCI USB1 Controller";
                        case 32:
                            return "EHCI USB2 Controller";
                        case 48:
                            return "XHCI USB3 Controller";
                        case 254:
                            return "USB Device";
                    }
                    break;
                case 4:
                    return "Fibre Channel Controller";
                case 5:
                    return "SMBus Controller";
                case 6:
                    return "InfiniBand Controller";
                case 7:
                    return "IPMI Interface Controller";
            }
            return "Serial Bus Controller";
        default:
            return "Unknown";
    }
}

static inline uint8_t is_multifunction(uint8_t bus, uint8_t slot) {
    uint8_t header_type = (uint8_t) (read_dword(bus, slot, 0, 0xC) >> 16);
    return header_type & (1 << 7);
}

static inline pci::device create_device(uint8_t bus, uint8_t slot, uint8_t func, int64_t parent) {
    return pci::device(
        bus,
        slot,
        func,
        get_class(bus, slot, func),
        get_subclass(bus, slot, func),
        get_prog_if(bus, slot, func),
        get_device(bus, slot, func),
        get_vendor(bus, slot, func),
        is_multifunction(bus, slot),
        parent
    );
}

static inline void scan_func(uint8_t bus, uint8_t slot, uint8_t func, int64_t parent);
static inline void scan_bus(uint8_t bus, int64_t parent) {
    for (size_t dev = 0; dev < pci::MAX_DEVICE; dev++) {
        for (size_t func = 0; func < pci::MAX_FUNCTION; func++) {
            scan_func(bus, dev, func, parent);
        }
    }
}

static inline void scan_func(uint8_t bus, uint8_t slot, uint8_t func, int64_t parent) {
    auto device = create_device(bus, slot, func, parent);

    uint32_t config_0 = device.readd(0);
    if (config_0 == 0xFFFFFFFF) {
        return;
    }

    pci::devices.push_back(device);

    if (device.get_clazz() == 0x06 && device.get_subclass() == 0x04) {
        uint32_t config_18 = device.readd(0x18);
        scan_bus((config_18 >> 8) & 0xFF, pci::devices.size());
    }
}

static inline void scan_root() {
    uint32_t config_c = read_dword(0, 0, 0, 0xC);
    uint32_t config_0;

    if (!(config_c & 0x800000)) {
        scan_bus(0, -1);
    } else {
        for (size_t func = 0; func < 8; func++) {
            config_0 = read_dword(0, 0, func, 0);
            if (config_0 == 0xffffffff)
                continue;

            scan_bus(func, -1);
        }
    }
}

void pci::init() {
    scan_root();

    kmsg("[PCI] Detected ", devices.size(), " devices");
    for (auto& device : devices) {
        kmsg("[PCI] Device: ", pci::to_string(device.get_clazz(), device.get_subclass(), device.get_prog_if()));
        kmsg("[PCI] Bus:       ", device.get_bus());
        kmsg("[PCI] Slot:    ", device.get_slot());
        kmsg("[PCI] Function:  ", device.get_func());
        kmsg("[PCI] Class:     ", device.get_clazz());
        kmsg("[PCI] Subclass:  ", device.get_subclass());
    }
}

pci::device *pci::get_device(uint8_t cl, uint8_t subcl, uint8_t prog_if) {
    auto len = devices.size();
    for (size_t i = 0; i < len; i++) {
        auto current_class    = devices[i].get_clazz();
        auto current_subclass = devices[i].get_subclass();
        auto current_prog_if  = devices[i].get_prog_if();

        if (current_class == cl && current_subclass == subcl &&
            current_prog_if == prog_if) {
            return &devices[i];
        }
    }

    return nullptr;
}