#include "smarter/smarter.hpp"
#include <fs/dev.hpp>
#include <fs/vfs.hpp>
#include <cstddef>
#include <driver/part.hpp>
#include <mm/mm.hpp>

size_t part::probe(vfs::devfs::blockdev *dev) {
    mbr::header *mbr_header = (mbr::header *) dev->allocator.allocate(dev->block_size);
    gpt::header *gpt_header = (gpt::header *) dev->allocator.allocate(dev->block_size);

    if (dev->read(gpt_header, dev->block_size, dev->block_size) < 1) {
        dev->allocator.deallocate(mbr_header);
        dev->allocator.deallocate(gpt_header);
        return -1;
    }

    if (gpt_header->sig == EFI_MAGIC) {
        if (gpt_header->part_size != sizeof(gpt::part)) {
            dev->allocator.deallocate(mbr_header);
            dev->allocator.deallocate(gpt_header);
            return -1;
        }

        gpt::part *gpt_part_list = (gpt::part *) dev->allocator.allocate(gpt_header->part_len * sizeof(gpt::part));
        if (dev->read(gpt_part_list, gpt_header->part_len * sizeof(gpt::part), gpt_header->part_start * dev->block_size) < 1) {
            dev->allocator.deallocate(mbr_header);
            dev->allocator.deallocate(gpt_header);
            dev->allocator.deallocate(gpt_part_list);

            return -1;
        }

        for (size_t i = 0; i < gpt_header->part_len; i++) {
            gpt::part part = gpt_part_list[i];
            if (part.uuid[0] == 0 && part.uuid[1] == 0) {
                continue;
            }

            dev->part_list.push({part.lba_end - part.lba_start, part.lba_start});
            auto private_data = prs::allocate_shared<vfs::devfs::dev_priv>(dev->allocator);
            private_data->dev = dev;
            private_data->part = dev->part_list.size() - 1;

            auto parent = dev->file->parent.lock();
            auto part_node = prs::allocate_shared<vfs::node>(dev->allocator, dev->file->fs, dev->file->name + (dev->part_list.size() + 48), parent, 0, vfs::node::type::BLOCKDEV);
            part_node->as_data(private_data);

            parent->children.push_back(part_node);
        }

        dev->allocator.deallocate(mbr_header);
        dev->allocator.deallocate(gpt_header);
        dev->allocator.deallocate(gpt_part_list);
        return 0;
    }

    if (dev->read(mbr_header, dev->block_size, 0) < 1) {
        dev->allocator.deallocate(mbr_header);
        dev->allocator.deallocate(gpt_header);
        return -1;
    }

    if (mbr_header->magic == 0xAA55) {
        mbr::part *mbr_part_list = (mbr::part *) mbr_header->parts;
        for (size_t i = 0; i < 4; i++) {
            mbr::part part = mbr_part_list[i];
            if (part.type == 0 || part.type == 0xEE) {
                continue;
            }

            dev->part_list.push({part.len, part.lba_start});
            auto private_data = prs::allocate_shared<vfs::devfs::dev_priv>(dev->allocator);
            private_data->dev = dev;
            private_data->part = dev->part_list.size() - 1;

            auto parent = dev->file->parent.lock();
            auto part_node = prs::allocate_shared<vfs::node>(dev->allocator, dev->file->fs, dev->file->name + (dev->part_list.size() + 48), parent, 0, vfs::node::type::BLOCKDEV);
            part_node->as_data(private_data);
            
            parent->children.push_back(part_node);
        }
    }

    dev->allocator.deallocate(mbr_header);
    dev->allocator.deallocate(gpt_header);
    return 0;
}