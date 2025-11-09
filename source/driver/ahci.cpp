#include "frg/allocation.hpp"
#include <cstddef>
#include <cstdint>
#include <driver/ahci.hpp>
#include <driver/part.hpp>
#include <fs/devfs.hpp>
#include <fs/vfs.hpp>
#include <mm/common.hpp>
#include <mm/mm.hpp>
#include <mm/pmm.hpp>
#include <sys/pci.hpp>
#include <util/log/log.hpp>
#include <util/string.hpp>

uint8_t port_type(volatile ahci::port *port) {
    uint32_t sata_status = port->sata_status;

    uint8_t interface_power_mgmt = (sata_status >> 8) & 0x0F;
    uint8_t detection = sata_status & 0x0F;

    if ((interface_power_mgmt != 0x1) || (detection != 0x03)) {
        return ahci::DEV_NULL;
    }

    switch (port->sig) {
        case ahci::SIG_ATAPI:
            return ahci::DEV_ATAPI;
        case ahci::SEG_SEMB:
            return ahci::DEV_SEMB;
        case ahci::SIG_PM:
            return ahci::DEV_PM;
        default:
            return ahci::DEV_ATA;
    }

    return ahci::DEV_NULL;
};

uint8_t port_present(volatile ahci::abar *bar, uint8_t port) {
    uint32_t port_implemented = bar->port_implemented;

    if (port_implemented & (1 << port)) {
        return port_type(&bar->ports[port]);
    }

    return ahci::DEV_NULL;
}

void await_ready(volatile ahci::port *port) {
    while (port->tfd & (1 << 3) && port->tfd & (1 << 7)) {
        asm volatile("pause");
    }
}

ahci::ssize_t find_cmdslot(volatile ahci::port *port) {
    uint32_t slots = (port->sata_active | port->cmd_issue);
    for (size_t i = 0; i < ahci::MAX_DEVICES; i++) {
        if (!(slots & (1 << i))) {
            return i;
        }
    }

    return -1;
}

ahci::command *get_command(ahci::port *port, uint8_t slot) {
    uint64_t header_address = (uint64_t) (port->clba);
    ahci::command *command = memory::common::offsetVirtual((ahci::command *) header_address);
    return command + slot;
}

void start_command(volatile ahci::port *port) {
    port->cas &= ~ahci::HBA_PxCMD_ST;

    while (port->cas & ahci::HBA_PxCMD_CR);

    port->cas |= ahci::HBA_PxCMD_FRE;
    port->cas |= ahci::HBA_PxCMD_ST;
}

void get_ownership(ahci::abar *bar) {
    uint16_t minor = bar->version & 0xFFFF;
    uint8_t  minor_2 = minor >> 8;
    uint16_t major = (bar->version >> 16) & 0xFFFF;

    if (major >= 1 && minor_2 >= 2) {
        if (bar->cap2 & 1) {
            bar->bohc |= (1 << 1);

            for (uint64_t i = 0; i < 100000; i++) {
                asm volatile("pause");
            }

            if (bar->bohc & (1 << 4)) {
                for (uint64_t i = 0; i < 800000; i++) {
                    asm volatile("pause");
                }
            }

            uint32_t bohc_actual = bar->bohc;
            if (!(bohc_actual & (1 << 1)) && (bohc_actual & (1 << 0) || bohc_actual & (1 << 4))) {
                panic("[AHCI] BIOS Refusing handoff");
            }

            bar->bohc |= (1 << 3);
        }
    }
    
    kmsg("[AHCI] Got handoff from BIOS");
}

ahci::ssize_t ahci::device::read_write(void *buf, uint16_t count, size_t offset, bool rw) {
    if (!exists) {
        return -vfs::error::IO;
    }
    
    ssize_t command_slot = find_cmdslot(port);
    if (command_slot == -1) {
        kmsg("[AHCI] No free command slots.");
        return -vfs::error::IO;
    }

    volatile ahci::command *command = memory::common::offsetVirtual((volatile ahci::command *) port->clba);
    command += command_slot;
    command->prdt_cnt = 1;
    command->write = rw;
    command->cmd_fis_len = sizeof(fis::reg_h2d) / sizeof(uint32_t);

    volatile ahci::command_table *command_table = memory::common::offsetVirtual((volatile ahci::command_table *) command->ctba);
    memset((void *) command_table, 0, sizeof(ahci::command_table));

    command_table->prdt_entry[0].dba = (uint32_t) (uint64_t) memory::common::removeVirtual(buf);
    command_table->prdt_entry[0].dbc = (count * sector_size);
    command_table->prdt_entry[0].interrupt = 1;

    fis::reg_h2d *command_fis = (fis::reg_h2d *) command_table->cfis;
    memset(command_fis, 0, sizeof(fis::reg_h2d));

    command_fis->fis_type = FIS_REG_H2D;
    command_fis->cmd_ctl = 1;
    if (rw) {
        if (lba48) {
            command_fis->command = ATA_COMMAND_DMA_EXT_WRITE;
        } else {
            command_fis->command = ATA_COMMAND_DMA_WRITE;
        }
    } else {
        if (lba48) {
            command_fis->command = ATA_COMMAND_DMA_EXT_READ;
        } else {
            command_fis->command = ATA_COMMAND_DMA_READ;
        }
    }

    command_fis->lba0 = offset & 0xFF;
    command_fis->lba1 = (offset >> 8) & 0xFF;
    command_fis->lba2 = (offset >> 16) & 0xFF;
    command_fis->dev = 0xA0 | (1 << 6);
    command_fis->control = 0x08;

    if (lba48) {
        command_fis->lba3 = (offset >> 24) & 0xFF;
        command_fis->lba4 = (offset >> 32) & 0xFF;
        command_fis->lba5 = (offset >> 40) & 0xFF;
    }

    if (count != 0xFFFF) {
        command_fis->countl = count & 0xFF;
        command_fis->counth = (count >> 8) & 0xFF;
    } else {
        command_fis->countl = 0;
        command_fis->counth = 0;
    }  

    port->cmd_issue = (1 << command_slot);

    while (true) {
        if (!(port->cmd_issue & (1 << command_slot))) {
            break;
        }

        if (port->ist & (1 << 30)) {
            kmsg("[AHCI] Disk IO Error.");
            return -vfs::error::IO;
        }
    }

    if (port->ist & (1 << 30)) {
        kmsg("[AHCI] Disk IO Error.");
        return -vfs::error::IO;
    }

    return count * sector_size;
}

void ahci::device::setup(volatile ahci::port *port) {
    port->clba = (uint32_t) (uint64_t) memory::common::removeVirtual(memory::pmm::alloc(1));
    port->clba_upper = 0;

    fis::hba *hba = (fis::hba *) kmalloc(sizeof(fis::hba));
    
    hba->dsfis.fis_type = FIS_DMA_STP;
    hba->psfis.fis_type = FIS_PIO_STP;
    hba->rfis.fis_type = FIS_REG_D2H;
    hba->sdbfis[0] = FIS_DEV_BTS;

    port->fis_addr = (uint32_t) (uint64_t) memory::common::removeVirtual(hba);
    port->fis_upper = 0;

    volatile ahci::command *command = memory::common::offsetVirtual((volatile ahci::command *) port->clba);

    for (uint64_t i = 0; i < MAX_DEVICES; i++) {
        command[i].prdt_cnt = 8;

        command[i].ctba = (uint32_t) (uint64_t) memory::common::removeVirtual(memory::pmm::alloc(1));
        command[i].ctba_upper = 0;
    }
}

void ahci::device::init(volatile ahci::port *port) {
    uint8_t *id = (uint8_t *) memory::pmm::alloc(1);
    size_t spin = 0;

    ssize_t slot = find_cmdslot(port);
    if (slot == -1) {
        kmsg("[AHCI] Failed to find free command slot for ATA_COMMAND_IDENTIFY.");
        return;
    }

    volatile ahci::command *command = memory::common::offsetVirtual((volatile ahci::command *) port->clba);

    command += slot;
    command->cmd_fis_len = sizeof(volatile fis::reg_h2d) / sizeof(uint32_t);
    command->write = 0;
    command->prdt_cnt = 1;

    volatile ahci::command_table *command_table = memory::common::offsetVirtual((volatile ahci::command_table *) command->ctba);
    memset((void *) command_table, 0, sizeof(ahci::command_table));

    command_table->prdt_entry[0].dba = (uint32_t) ((uint64_t) memory::common::removeVirtual(id));
    command_table->prdt_entry[0].dbc = 511;
    command_table->prdt_entry[0].interrupt = 1;

    volatile fis::reg_h2d *command_fis = (fis::reg_h2d *) (command_table->cfis);
    memset((void *) command_fis, 0, sizeof(fis::reg_h2d));

    command_fis->command = ATA_COMMAND_IDENTIFY;
    command_fis->cmd_ctl = 1;
    command_fis->dev = 0;
    command_fis->pmport = 0;
    command_fis->fis_type = FIS_REG_H2D;

    while ((port->tfd & (ATA_DEVICE_BUSY | ATA_DEVICE_DRQ)) && spin < 1000000) {
        spin++;
    }

    if (spin >= 1000000) {
        kmsg("[AHCI] Port hung");
    }

    await_ready(port);
    start_command(port);
    port->cmd_issue = 1 << slot;
    
    while (true) {
        if (!(port->cmd_issue & (1 << slot))) {
            break;
        }

        if (port->ist & (1 << 30)) {
            kmsg("[AHCI] Disk read error during Identification.");
            memory::pmm::free(id, 1);
            return;
        }
    }    

    if (port->ist & (1 << 30)) {
        kmsg("[AHCI] Disk read error during Identification.");
        memory::pmm::free(id, 1);
        return;
    }

    uint64_t data_valid = *(uint16_t *) (&id[212]);

    if (!(data_valid & (1 << 5)) && (data_valid & (1 << 14)) && (data_valid & (1 << 12))) {
        uint32_t real_sector_size = *(uint32_t *) (&id[234]);
        sector_size = real_sector_size;
    } else {
        sector_size = 512;
    }

    exists = true;
    this->port = port;
    sectors = *(uint64_t *) (&id[200]);

    lba48 = (id[167] & (1 << 2)) && (id[173] & (1 << 2));

    if (!sectors) {
        sectors = *(uint32_t *) (&id[120]);
    }

    if (lba48) {
        kmsg("[AHCI] LBA48 Supported");
    }

    kmsg("[AHCI] Sectors: ", sectors);
    kmsg("[AHCI] Sector size: ", sector_size);
    kmsg("[AHCI] Identify suceeded");

    memory::pmm::free(id, 1);

    block_size = sector_size;
    blocks = sectors;
    major = vfs::devfs::STORAGE_MAJOR;
    minor = this->id;
    name = vfs::path{"sd"} + alphabet[this->id];
}

ahci::ssize_t ahci::device::read(void *buf, ssize_t count, ssize_t offset) {
    void *tmp = kcalloc(1, count);

    if (count >= 0xFFFF) {
        return -vfs::error::IO;
    } else if (count == 0) {
        return -vfs::error::IO;
    }

    uint64_t sector_offset = offset % sector_size;
    uint64_t sector_start = offset / sector_size;
    uint64_t sector_end = (offset + count) / sector_size;
    uint64_t sector_count = sector_end - sector_start;
    
    ssize_t err = 0;
    if ((err = read_write(tmp, sector_count, sector_start, false)) != count) {
        kfree(tmp);
        kmsg("[AHCI] Failed to read ", count, " Bytes from disk ", id, " error code ", err);
        return -vfs::error::IO;;
    }

    memcpy(buf, tmp + sector_offset, count);
    kfree(tmp);
    return err;
}

ahci::ssize_t ahci::device::write(void *buf, ssize_t count, ssize_t offset) {
    uint64_t sector_offset = offset % sector_size;
    uint64_t sector_start = offset / sector_size;
    uint64_t sector_end = ((offset + count) + sector_size - 1) / sector_size;
    uint64_t sector_count = sector_end - sector_start;

    if (sector_count > 0xFFFF) {
        return -vfs::error::INVAL;
    } else if (!sector_count) {
        return -vfs::error::INVAL;
    }

    uint8_t *dst = (uint8_t *) kmalloc(sector_count * sector_size);
    uint8_t *dst_end = dst + ((sector_count - 1) * sector_size);

    ssize_t err = 0;
    if ((err = read_write(dst, 1, sector_start, true)) != 0) {
        kfree(dst);
        return -vfs::error::IO;;
    }

    if ((err = read_write(dst_end, 1, sector_end - 1, true)) != 0) {
        kfree(dst);
        return -vfs::error::IO;;
    }

    memcpy(buf, dst + sector_offset, count);

    if ((err = read_write(dst, sector_count, sector_start, true)) != 0) {
        kfree(dst);
        return -vfs::error::IO;;
    }

    kfree(dst);
    return err;
}

ahci::ssize_t ahci::device::ioctl(size_t req, void *buf) {
    switch (req) {
        case vfs::devfs::BLKRRPART:
            part::probe(this);
            return 0;
        default:
            return -vfs::error::INVAL;
    }
}

void ahci::init() {
    auto pci_device = pci::get_device(AHCI_CLASS, AHCI_SUBCLASS, AHCI_PROG_IF);
    if (!pci_device) {
        kmsg("[AHCI] No AHCI Controller avalilable.");
        return;
    }

    pci_device->enable_busmastering();

    pci::bar pci_bar;
    pci_device->read_bar(5, pci_bar);
    if (!pci_bar.valid || !pci_bar.is_mmio || !pci_bar.base) {
        kmsg(pci_bar.valid, ", ", pci_bar.is_mmio, ", ", util::hex(pci_bar.base));
        kmsg("[AHCI] AHCI ABAR Invalid!");
        return;
    }

    volatile ahci::abar *bar = memory::common::offsetVirtual((volatile ahci::abar *) pci_bar.base);
    kmsg("[AHCI] ABAR Base: ", bar);

    get_ownership((ahci::abar *) bar);
    bar->ghc |= (1 << 31);

    auto found_devices = false;
    for (size_t i = 0; i < MAX_DEVICES; i++) {
        switch (port_present(bar, i)) {
            case DEV_PM:
            case DEV_SEMB:
            case DEV_ATAPI:
                kmsg("[AHCI] Found Unsupported AHCI device with port id ", i);
                break;
            case DEV_ATA: {
                found_devices = true;
                kmsg("[AHCI] Found SATA device with port id ", i);
                
                auto device = frg::construct<ahci::device>(memory::mm::heap);
                device->id = i;
                device->setup(&bar->ports[i]);
                device->init(&bar->ports[i]);                
                part::probe(device);

                vfs::devfs::add(device);
                break;
            }
            default:
                break;
        }
    }

    if (!found_devices) {
        kmsg("[AHCI] No AHCI Drives found");
    }
}