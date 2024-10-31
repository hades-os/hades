#include "driver/part.hpp"
#include "frg/string.hpp"
#include "frg/tuple.hpp"
#include "mm/mm.hpp"
#include "sys/smp.hpp"
#include "util/log/log.hpp"
#include "util/log/panic.hpp"
#include <cstddef>
#include <frg/allocation.hpp>
#include <fs/vfs.hpp>
#include <fs/dev.hpp>

void vfs::devfs::init() {
    vfs::mkdir(nullptr, "/dev", 0, mode::RDWR);
    vfs::mount("/", "/dev", fslist::DEVFS, mflags::NOSRC);

    vfs::mkdir(nullptr, "/dev/pts", 0, mode::RDWR);
    kmsg("[VFS] Initial devfs mounted.");
}

void vfs::devfs::add(frg::string_view path, device *dev) {
    // TODO: device addition
    node *device_node = vfs::make_node(vfs::device_fs()->root, path, dev->blockdev ? node::type::BLOCKDEV : node::type::CHARDEV);
    if (!device_node) {
        panic("[DEVFS]: Unable to make device: ", path.data());
    }

    auto private_data = frg::construct<dev_priv>(memory::mm::heap);
    private_data->dev = dev;
    private_data->part = -1;

    device_node->private_data = private_data;
    dev->file = device_node;
}

vfs::node *vfs::devfs::lookup(node *parent, frg::string_view name) {
    auto node = parent->find_child(name);
    if (!node) return nullptr;

    auto private_data = (dev_priv *) node->private_data;
    if (!private_data) return nullptr;

    devfs::device *device = private_data->dev;
    if (!device || !device->resolveable) return nullptr;

    return node;
}

vfs::ssize_t vfs::devfs::on_open(vfs::fd *fd, ssize_t flags) {
    auto device = (devfs::device *) fd->desc->node->private_data;
    return device->on_open(fd, flags);
}

vfs::ssize_t vfs::devfs::on_close(vfs::fd *fd, ssize_t flags) {
    auto device = (devfs::device *) fd->desc->node->private_data;
    return device->on_open(fd, flags);

    return device->on_close(fd, flags);
}

bool vfs::devfs::request_io(node *file, device::block_zone *zones, size_t num_zones, bool rw, bool all_success) {
    auto private_data = (dev_priv *) file->private_data;
    if (!private_data) return false;

    devfs::device *device = private_data->dev;
    if (!device || !device->resolveable) return false;

    if (!device->blockdev) {
        return false;
    }

    if (private_data->part >= 0) {
        auto part = device->block.part_list.data()[private_data->part];
        device->block.request_io(device->block.request_data, zones, num_zones, (part.begin * device->block.block_size), rw);
        device->block.mail->recv(true, rw ? device::BLOCK_IO_WRITE : device::BLOCK_IO_READ, smp::get_thread());

        size_t success_count = 0;
        for (auto res_zone = zones; res_zone != nullptr; res_zone = res_zone->next) {
            if (res_zone->is_success) success_count++;
        }

        if (all_success && success_count != num_zones) return false;
        return true;
    }

    device->block.request_io(device->block.request_data, zones, num_zones, 0, rw);
    device->block.mail->recv(true, rw ? device::BLOCK_IO_WRITE : device::BLOCK_IO_READ, smp::get_thread());

    size_t success_count = 0;
    for (auto res_zone = zones; res_zone != nullptr; res_zone = res_zone->next) {
        if (res_zone->is_success) success_count++;
    }

    if (all_success && success_count != num_zones) return false;
    return true;
}


vfs::ssize_t vfs::devfs::read(node *file, void *buf, size_t len, size_t offset) {
    auto private_data = (dev_priv *) file->private_data;
    if (!private_data) return -error::NOENT;

    devfs::device *device = private_data->dev;
    if (!device) return -error::NOENT;

    if (device->blockdev) {
        if (private_data->part >= 0) {
            auto part = device->block.part_list.data()[private_data->part];
            return device->read(buf, len, offset + (part.begin * device->block.block_size));
        }
    }

    return device->read(buf, len, offset);
}

vfs::ssize_t vfs::devfs::write(node *file, void *buf, size_t len, size_t offset) {
    auto private_data = (dev_priv *) file->private_data;
    if (!private_data) return -error::NOENT;

    devfs::device *device = private_data->dev;
    if (!device) return -error::NOENT;

    if (device->blockdev) {
        if (private_data->part >= 0) {
            auto part = device->block.part_list.data()[private_data->part];
            return device->write(buf, len, offset + (part.begin * device->block.block_size));
        }
    }

    return device->write(buf, len, offset);
}

vfs::ssize_t vfs::devfs::ioctl(node *file, size_t req, void *buf) {
    auto device = (devfs::device *) file->private_data;
    if (!device) {
        return -error::NOENT;
    }

    switch (req) {
        case BLKRRPART:
            if (!device->blockdev) return -1;
            device->block.part_list.clear();
            return part::probe(device);
        default:
            return device->ioctl(req, buf);
    }
}

void *vfs::devfs::mmap(node *file, void *addr, size_t len, size_t offset) {
    auto device = (devfs::device *) file->private_data;
    if (!device) {
        return nullptr;
    }

    return device->mmap(file, addr, len, offset);
}

vfs::ssize_t vfs::devfs::mkdir(node *dst, frg::string_view name, int64_t flags) {
    node *new_dir = frg::construct<vfs::node>(memory::mm::heap, this, name, dst, flags, node::type::DIRECTORY);
    dst->children.push_back(new_dir);

    return 0;
}