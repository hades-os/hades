#include "util/log/log.hpp"
#include <cstddef>
#include <cstdint>
#include <driver/part.hpp>
#include <mm/mm.hpp>

size_t part::probe(vfs::devfs::device *dev) {
    if (!dev->blockdev) {
        return -1;
    }

    mbr::header *mbr_header = (mbr::header *) kmalloc(dev->block.block_size);
    gpt::header *gpt_header = (gpt::header *) kmalloc(dev->block.block_size);

    if (dev->read(gpt_header, dev->block.block_size, dev->block.block_size) < 1) {
        kfree(mbr_header);
        kfree(gpt_header);
        return -1;
    }

    if (gpt_header->sig == EFI_MAGIC) {
        if (gpt_header->part_size != sizeof(gpt::part)) {
            kfree(mbr_header);
            kfree(gpt_header);
            return -1;
        }

        gpt::part *gpt_part_list = (gpt::part *) kmalloc(gpt_header->part_len * sizeof(gpt::part));
        if (dev->read(gpt_part_list, gpt_header->part_len * sizeof(gpt::part), gpt_header->part_start * dev->block.block_size) < 1) {
            kfree(mbr_header);
            kfree(gpt_header);
            kfree(gpt_part_list);

            return -1;
        }

        for (size_t i = 0; i < gpt_header->part_len; i++) {
            gpt::part part = gpt_part_list[i];
            if (part.uuid[0] == 0 && part.uuid[1] == 0) {
                continue;
            }

            dev->block.part_list.push({part.lba_end - part.lba_start, part.lba_start});
        }

        kfree(mbr_header);
        kfree(gpt_header);
        kfree(gpt_part_list);
        return 0;
    }

    if (dev->read(mbr_header, dev->block.block_size, 0) < 1) {
        kfree(mbr_header);
        kfree(gpt_header);
        return -1;
    }

    if (mbr_header->magic == 0xAA55) {
        mbr::part *mbr_part_list = (mbr::part *) mbr_header->parts;
        for (size_t i = 0; i < 4; i++) {
            mbr::part part = mbr_part_list[i];
            if (part.type == 0 || part.type == 0xEE) {
                continue;
            }

            dev->block.part_list.push({part.len, part.lba_start});
        }
    }

    kfree(mbr_header);
    kfree(gpt_header);
    return 0;
}