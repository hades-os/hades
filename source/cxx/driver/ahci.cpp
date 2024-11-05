#include "mm/vmm.hpp"
#include "sys/irq.hpp"
#include "sys/sched/wait.hpp"
#include "sys/smp.hpp"
#include <cstddef>
#include <cstdint>
#include <driver/ahci.hpp>
#include <driver/part.hpp>
#include <fs/dev.hpp>
#include <fs/vfs.hpp>
#include <frg/allocation.hpp>
#include <mm/common.hpp>
#include <mm/mm.hpp>
#include <mm/pmm.hpp>
#include <sys/pci.hpp>
#include <util/log/log.hpp>
#include <util/log/panic.hpp>
#include <util/string.hpp>

int AHCI_MAJOR = 0xA;
sched::thread *ahci_thread;

uint8_t port_type(volatile ahci::port *port) {
    uint32_t sata_status = port->sata_status;
    uint8_t detection = sata_status & 0x0F;
    if (detection != 3) {
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
    while (port->tfd & (1 << 3) || port->tfd & (1 << 7)) {
        asm volatile("pause");
    }
}

void issue_command(volatile ahci::port *port, size_t slot) {
    port->cmd_issue |= (1 << slot);
}

ahci::ssize_t ahci::find_cmdslot(ahci::device *device) {
    uint32_t slots = device->last_issued_cmdset;
    for (size_t i = 0; i < ahci::MAX_SLOTS; i++) {
        if (!(slots & (1 << i))) {
            return i;
        }
    }

    return -1;
}

void free_command(ahci::command_entry *command) {
    kfree(command);
}

ahci::command_slot get_command(volatile ahci::port *port, volatile ahci::abar *bar, ahci::device *device, uint64_t fis_size) {
    ahci::command_slot slot = {-1, nullptr};
    auto slot_idx = find_cmdslot(device);
    if (slot_idx == -1) {
        return slot;
    }

    ahci::command_entry *entry = (ahci::command_entry *) kmalloc(fis_size);

    slot.idx = slot_idx;
    slot.entry = entry;

    uint64_t header_address = (port->commands_addr | ((uint64_t) port->commands_addr_upper << 32));
    ahci::command_header *headers = memory::common::offsetVirtual((ahci::command_header *) header_address);

    headers[slot_idx].command_entry = (uint32_t) ((uint64_t) entry - memory::common::virtualBase);
    uint8_t is_long = (uint8_t) ((bar->cap & (1 << 31)) >> 31);

    if (is_long) {
        headers[slot_idx].command_entry_hi = (uint32_t) (((uint64_t) entry - memory::common::virtualBase) >> 32);
    } else {
        headers[slot_idx].command_entry_hi = 0;
    }

    return slot;
}

ahci::command_header *get_header(volatile ahci::port *port, uint8_t slot) {
    uint64_t header_address = (port->commands_addr | ((uint64_t) port->commands_addr_upper << 32));
    ahci::command_header *headers = memory::common::offsetVirtual((ahci::command_header *) header_address);

    return headers + slot;
}

void comreset(volatile ahci::port *port) {
    port->sata_ctl = (port->sata_ctl & ~(0b1111)) | 0x1;
    for (int i = 0; i < 100000; i++) asm volatile ("pause");
    port->sata_ctl = port->sata_ctl & (0b1111);
}

int wait_command(volatile ahci::port *port, size_t slot) {
    while (port->cmd_issue & (1 << slot)) {
        asm volatile("pause");
    }

    if (port->tfd & (1 << 0)) {
        return 1;
    }

    return 0;
}

void reset_engine(volatile ahci::port *port) {
    port->cas &= ahci::HBA_PxCMD_ST;
    while (port->cas & ahci::HBA_PxCMD_CR) asm volatile("pause");

    if (port->tfd & (1 << 7) || port->tfd & (1 << 3)) {
        comreset(port);
    }

    port->cas |= (1 << 0);
    while (!(port->cas & ahci::HBA_PxCMD_CR)) asm volatile("pause");
}

void stop_command(volatile ahci::port *port) {
    port->cas &= ~ahci::HBA_PxCMD_ST;
    port->cas &= ~ahci::HBA_PxCMD_FRE;

    while ((port->cas & ahci::HBA_PxCMD_CR) || (port->cas & ahci::HBA_PxCMD_FR));
}

void start_command(volatile ahci::port *port) {
    while (port->cas & ahci::HBA_PxCMD_CR);

    port->cas |= ahci::HBA_PxCMD_ST;
    port->cas |= ahci::HBA_PxCMD_FRE;
}

void fill_prdt(volatile ahci::port *port, volatile ahci::abar *bar, void *mem, ahci::prdt_entry *prdt) {
    prdt->base = (uint64_t) memory::common::removeVirtual(mem) & 0xFFFFFFFF;

    uint8_t is_long = (uint8_t) ((bar->cap & (1 << 31)) >> 31);
    if (is_long) {
        prdt->base_hi = (uint32_t) ((uint64_t) memory::common::removeVirtual(mem) >> 32);
    } else {
        prdt->base_hi = 0;
    }
}

void get_ownership(volatile ahci::abar *bar) {
    if (!(bar->cap2 & (1 << 0))) {
        kmsg("[AHCI] BIOS Handoff not supported");
        return;
    }

    bar->bohc |= (1 << 1);
    while ((bar->bohc & (1 << 0)) == 0) asm volatile("pause");

    for (int i = 0; i < 0x800000; i++) asm volatile("pause");

    if (bar->bohc & (1 << 4)) {
        for (int i = 0; i < 0x800000; i++) asm volatile("pause");
    }

    uint32_t bohc = bar->bohc;
    if (bohc & (1 << 4) || bohc & (1 << 0) || (bohc & (1 << 1)) == 0) {
        panic("[AHCI]: Unable to get BIOS handoff");
    }

    kmsg("[AHCI]: BIOS handoff successful");
}

void ahci::device::setup() {
    __atomic_store_n(&num_free_commands, ((bar->cap >> 8) & 0xF0), __ATOMIC_RELAXED);
    for (size_t i = 0; i < num_free_commands; i++) {
        last_issued_cmdset &= ~(1 << i);
    }

    stop_command(port);
    uint64_t data_base = (uint64_t) memory::common::removeVirtual(memory::pmm::alloc(1));
    uint64_t fis_base = data_base + (32 * 32);

    if (bar->cap & (1 << 31)) {
        port->commands_addr = data_base & 0xFFFFFFFF;
        port->commands_addr_upper = (data_base >> 32) & 0xFFFFFFFF;

        port->fis_addr = fis_base & 0xFFFFFFFF;
        port->fis_upper = (fis_base >> 32) & 0xFFFFFFFF;
    } else {
        port->commands_addr = data_base & 0xFFFFFFFF;
        port->commands_addr_upper = 0;

        port->fis_addr = fis_base & 0xFFFFFFFF;
        port->fis_upper = 0;
    }

    start_command(port);
    if (port->sig == SIG_ATA) {
        kmsg("[AHCI] Found ATA Device");
        identify_sata();

        major = AHCI_MAJOR;
        minor = this->id;
    }
}

void ahci::device::identify_sata() {
    ahci::command_slot slot = get_command(port, bar, this, get_fis_size(1));
    ahci::command_header *header = get_header(port, slot.idx);

    if (slot.idx == -1) {
        kmsg("[AHCI]: Could not find ATA slot");
        return;
    }

    header->prdt_cnt = 1;
    header->write = 0;
    header->cmd_fis_len = 5;

    fis::reg_h2d *fis_area = (fis::reg_h2d *) slot.entry->cfis;
    fis_area->fis_type = FIS_REG_H2D;
    fis_area->cmd_ctl = 1;
    fis_area->command = ATA_COMMAND_IDENTIFY;
    fis_area->dev = 0xA0;
    fis_area->control = ATA_DEVICE_DRQ;

    uint8_t *id_mem = (uint8_t *) memory::pmm::alloc(1);
    ahci::prdt_entry *prdt = (ahci::prdt_entry *) &(slot.entry->prdts[0]);
    fill_prdt(port, bar, id_mem, prdt);
    prdt->bytes = get_prdt_bytes(512);

    await_ready(port);
    issue_command(port, slot.idx);
    int err = wait_command(port, slot.idx);

    if (err) {
        uint8_t error = (uint8_t) (port->tfd >> 8);
        kmsg("[AHCI] Identify Error: ", error);

        reset_engine(port);
        free_command(slot.entry);
        return;
    }

    if (port->tfd & (1 << 0)) {
        uint8_t error = (uint8_t) (port->tfd >> 8);
        kmsg("[AHCI] Identify Error: ", error);

        reset_engine(port);
        free_command(slot.entry);

        exists = false;
        memory::pmm::free(id_mem);
        return;
    }

    uint16_t valid = *(uint16_t *) (&id_mem[212]);
    if (!(valid & (1<<15)) && (valid & (1<<14)) && (valid & (1<<12))) {
        sector_size = *(uint32_t *) (&id_mem[234]);
    } else {
        sector_size = 512;
    }

    sectors = *(uint64_t *) (&id_mem[200]);
    if (!sectors) {
        sectors = (uint64_t) (*(uint32_t *) (&id_mem[120]));
    }

    lba48 = (id_mem[167] & (1 << 2)) && (id_mem[173] & (1 << 2));

    exists = true;
    blockdev.block_size = sector_size;
    blockdev.blocks = sectors;
    blockdev.extra_data = this;
    blockdev.request_io = request_io;

    free_command(slot.entry);
    memory::pmm::free(id_mem);

    kmsg("[AHCI] Identify succeeded");
}

void ahci::request_io(void *extra_data, device::io_request *req, size_t part_offset, bool rw) {
    auto device = (ahci::device *) extra_data;
    volatile bool completed = false;

    device->lock.irq_acquire();
    auto request_id = device->last_request_id++;
    for (size_t i = 0; i < req->len; i++) {
        auto zone = req->blocks[i];
        device->requests.push_back({
            .zone = zone,
            .request_id = request_id,
            .completion_flag = &completed,
            .part_offset = part_offset,
            .rw = rw,
        });

    }

    device->lock.irq_release();
    while (!completed) {
        asm volatile("pause");
    }
}

ahci::command_slot ahci::device::issue_read_write(void *buf, uint16_t count, size_t offset, bool rw) {
    if (!exists) {
        return { .idx = -1 };
    }

    uint64_t prdt_count = ((count * sector_size) + 0x400000 - 1) / 0x400000;
    ahci::command_slot slot = get_command(port, bar, this, get_fis_size(prdt_count + 1));
    ahci::command_header *header = get_header(port, slot.idx);

    if (slot.idx == -1) {
        kmsg("[AHCI] No free command slots.");
        free_command(slot.entry);

        return slot;
    }

    __atomic_fetch_sub(&num_free_commands, 1, __ATOMIC_RELAXED);
    header->prdt_cnt = prdt_count;
    header->write = rw;
    header->cmd_fis_len = 5;

    fis::reg_h2d *fis_area = (fis::reg_h2d *) slot.entry->cfis;
    fis_area->fis_type = FIS_REG_H2D;
    fis_area->cmd_ctl = 1;
    fis_area->command = rw ? (lba48 ? ATA_COMMAND_DMA_EXT_WRITE : ATA_COMMAND_DMA_WRITE) :
                        (lba48 ? ATA_COMMAND_DMA_EXT_READ : ATA_COMMAND_DMA_READ);
    fis_area->dev = 0xA0 | (1 << 6);
    fis_area->control = ATA_DEVICE_DRQ;

    fis_area->lba0 = (offset >> 0) & 0xFF;
    fis_area->lba1 = (offset >> 8) & 0xFF;
    fis_area->lba2 = (offset >> 16) & 0xFF;

    if (lba48) {
        fis_area->lba3 = (offset >> 24) & 0xFF;
        fis_area->lba4 = (offset >> 32) & 0xFF;
        fis_area->lba5 = (offset >> 40) & 0xFF;
    }

    if (count != 0xFFFF) {
        fis_area->countl = (count >> 0) & 0xFF;
        fis_area->counth = (count >> 8) & 0xFF;
    } else {
        fis_area->countl = 0;
        fis_area->counth = 0;
    }

    char *data = (char *) buf;
    uint64_t rest = count * sector_size;
    for (uint64_t i = 0; i < prdt_count; i++){
        ahci::prdt_entry *prdt = (ahci::prdt_entry *) &(slot.entry->prdts[i]);
        fill_prdt(port, bar, data + (i * 0x400000), prdt);

        if (rest >= 0x400000) {
            prdt->bytes = get_prdt_bytes(0x400000);
            rest -= 0x400000;
        } else {
            prdt->bytes = get_prdt_bytes(rest);
            rest = 0;
        }
    }

    return slot;
}

void ahci::device::handle_commands() {
    lock.irq_acquire();
    // handle completed commands first
    for (size_t idx = 0; idx < 32; idx++) {
        // handle finished command
        if (!(port->cmd_issue & (1 << idx)) && (last_issued_cmdset & (1 << idx))) {
            // we had an oopsie
            auto command = active_commands[idx];
            last_issued_cmdset &= ~(1 << idx);
            if (port->tfd & (1 << 0)) {
                uint8_t error = (uint8_t) (port->tfd >> 8);
                kmsg("[AHCI] Transfer Error: ", error);

                reset_engine(port);
                free_command(command.slot.entry);
                memory::pmm::free(command.tmp);

                finished_map.contains(command.request_id)
                    ? finished_map[command.request_id]++
                    : finished_map[command.request_id] = 1;
                if (finished_map[command.request_id] == command.zone->req->len) {
                    *command.completion_flag = true;
                }

                __atomic_fetch_add(&num_free_commands, 1, __ATOMIC_RELAXED);

                continue;
            }

            // command successful
            command.zone->is_success = true;
            free_command(command.slot.entry);

            uint64_t sector_offset = (command.zone->offset + command.part_offset) % sector_size;
            memcpy(command.zone->buf, (char *) command.tmp + sector_offset, command.zone->len);

            memory::pmm::free(command.tmp);
            finished_map.contains(command.request_id)
                ? finished_map[command.request_id]++
                : finished_map[command.request_id] = 1;
            if (finished_map[command.request_id] == command.zone->req->len) {
                *command.completion_flag = true;
            }
            __atomic_fetch_add(&num_free_commands, 1, __ATOMIC_RELAXED);
        }
    }

    // if there are free command slots, enqueue, else, exit
    while (__atomic_load_n(&num_free_commands, __ATOMIC_RELAXED) > 0 && requests.size() > 0) {
        auto command_request = requests.pop();
        auto zone = command_request.zone;

        size_t offset = zone->offset + command_request.part_offset;
        uint64_t sector_start = offset / sector_size;
        uint64_t sector_end = ((offset + zone->len) + sector_size - 1) / sector_size;
        uint64_t sector_count = sector_end - sector_start;

        auto tmp = memory::pmm::alloc(((sector_count * sector_size) / memory::common::page_size) + 1);
        auto slot = issue_read_write(tmp, sector_count, sector_start, command_request.rw);
        if (slot.idx == -1) {
            memory::pmm::free(tmp);
            requests.push(command_request);

            break;
        }

        await_ready(port);
        issue_command(port, slot.idx);
        last_issued_cmdset |= (1 << slot.idx);

        active_commands[slot.idx] = command_request;
        active_commands[slot.idx].slot = slot;
        active_commands[slot.idx].tmp = tmp;
    }


    lock.irq_release();
}

ahci::ssize_t ahci::device::read(void *buf, size_t count, size_t offset) {
    lock.irq_acquire();

    uint64_t sector_offset = offset % sector_size;
    uint64_t sector_start = offset / sector_size;
    uint64_t sector_end = ((offset + count) + sector_size - 1) / sector_size;
    uint64_t sector_count = sector_end - sector_start;

    if (sector_count > 0xFFFF) {
        lock.irq_release();
        return -vfs::error::IO;
    } else if (sector_count == 0) {
        lock.irq_release();
        return -vfs::error::IO;
    }

    void *tmp = kmalloc(sector_count * sector_size);
    auto slot = issue_read_write(tmp, sector_count, sector_start, false);
    await_ready(port);

    last_issued_cmdset |= (1 << slot.idx);

    issue_command(port, slot.idx);
    wait_command(port, slot.idx);

    last_issued_cmdset &= ~(1 << slot.idx);
    if (port->tfd & (1 << 0)) {
        uint8_t error = (uint8_t) (port->tfd >> 8);
        kmsg("[AHCI] Transfer Error: ", error);

        reset_engine(port);
        free_command(slot.entry);
        __atomic_fetch_add(&num_free_commands, 1, __ATOMIC_RELAXED);

        kfree(tmp);
        lock.irq_release();
        return -vfs::error::IO;
    }

    free_command(slot.entry);
    __atomic_fetch_add(&num_free_commands, 1, __ATOMIC_RELAXED);
    memcpy(buf, (char *) tmp + sector_offset, count);

    kfree(tmp);
    lock.irq_release();
    return count;
}

ahci::ssize_t ahci::device::write(void *buf, size_t count, size_t offset) {
    return -1;
}

static frg::vector<ahci::device *, memory::mm::heap_allocator> devices{};
void ahci::ahci_task() {
    while (true) {
        for (auto device: devices) {
            if (device == nullptr) continue;
            if (device->exists) {
                device->handle_commands();
            }
        }
    }
}

void ahci::init() {
    auto pci_device= pci::get_device(AHCI_CLASS, AHCI_SUBCLASS, AHCI_PROG_IF);
    if (!pci_device) {
        kmsg("[AHCI] No AHCI Controller avalilable.");
        return;
    }

    pci_device->enable_mmio();
    pci_device->enable_busmastering();

    pci::bar pci_bar;
    pci_device->read_bar(5, pci_bar);
    if (!pci_bar.valid || !pci_bar.is_mmio || !pci_bar.base) {
        kmsg(pci_bar.valid, ", ", pci_bar.is_mmio, ", ", util::hex(pci_bar.base));
        kmsg("[AHCI] AHCI ABAR Invalid!");
        return;
    }

    volatile ahci::abar *ahci_bar = memory::common::offsetVirtual((ahci::abar *) pci_bar.base);
    kmsg("[AHCI] ABAR Base: ", ahci_bar);

    ahci_bar->ghc |= (1 << 31);
    get_ownership(ahci_bar);

    auto found_devices = false;
    for (size_t i = 0; i < MAX_SLOTS; i++) {
        switch (port_present(ahci_bar, i)) {
            case DEV_PM:
            case DEV_SEMB:
            case DEV_ATAPI:
                kmsg("[AHCI] Found Unsupported AHCI device with port id ", i);
                break;
            case DEV_ATA: {
                found_devices = true;
                kmsg("[AHCI] Found SATA device with port id ", i);

                auto device = frg::construct<ahci::device>(memory::mm::heap);
                device->is_blockdev = true;
                device->bar = ahci_bar;
                device->id = i;
                device->port = &ahci_bar->ports[i];
                device->setup();
                devices.push_back(device);

                vfs::devfs::add(vfs::path{"sd"} + alphabet[device->id], device);
                part::probe(device);

                break;
            }
            default:
                break;
        }
    }

    if (!found_devices) {
        kmsg("[AHCI] No AHCI Drives found");
    } else {
        ahci_thread = sched::create_thread(&ahci_task, (uint64_t) memory::pmm::stack(4), (memory::vmm::vmm_ctx *) memory::vmm::boot(), 0);
        ahci_thread->start();
    }
}