#include "driver/part.hpp"
#include "frg/string.hpp"
#include "mm/mm.hpp"
#include "util/log/log.hpp"
#include "util/log/panic.hpp"
#include <cstddef>
#include <frg/allocation.hpp>
#include <fs/vfs.hpp>
#include <fs/dev.hpp>

void vfs::devfs::init() {
    vfs::mkdir(nullptr, "/dev", 0, O_RDWR);
    vfs::mount("/", "/dev", fslist::DEVFS, mflags::NOSRC);

    vfs::mkdir(nullptr, "/dev/pts", 0, O_RDWR);
    kmsg("[VFS] Initial devfs mounted.");
}

void vfs::devfs::add(frg::string_view path, device *dev) {
    // TODO: device addition
    node *device_node = vfs::make_node(vfs::device_fs()->root, path, dev->is_blockdev ? node::type::BLOCKDEV : node::type::CHARDEV);
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

ssize_t vfs::devfs::on_open(vfs::fd *fd, ssize_t flags) {
    auto device = (devfs::device *) fd->desc->node->private_data;
    return device->on_open(fd, flags);
}

ssize_t vfs::devfs::on_close(vfs::fd *fd, ssize_t flags) {
    auto device = (devfs::device *) fd->desc->node->private_data;
    return device->on_open(fd, flags);

    return device->on_close(fd, flags);
}

void vfs::devfs::request_io(node *file, device::io_request *req, bool rw, bool all_success) {
    auto private_data = (dev_priv *) file->private_data;
    if (!private_data) return;

    devfs::device *device = private_data->dev;
    if (!device || !device->resolveable) return;

    if (!device->is_blockdev) {
        return;
    }

    size_t part_offset = 0;
    if (private_data->part >= 0) {
        auto part = device->blockdev.part_list.data()[private_data->part];
        part_offset = (part.begin * device->blockdev.block_size);
    }

    device->blockdev.request_io(device->blockdev.extra_data, req, part_offset, rw);
    for (size_t i = 0; i < req->len; i++) {
        frg::destruct(memory::mm::heap, req->blocks[i]);
    }

    kfree(req->blocks);
}


ssize_t vfs::devfs::read(node *file, void *buf, size_t len, off_t offset) {
    auto private_data = (dev_priv *) file->private_data;
    if (!private_data) return -ENOENT;

    devfs::device *device = private_data->dev;
    if (!device) return -ENOENT;

    if (device->is_blockdev) {
        if (private_data->part >= 0) {
            auto part = device->blockdev.part_list.data()[private_data->part];
            return device->read(buf, len, offset + (part.begin * device->blockdev.block_size));
        }
    }

    return device->read(buf, len, offset);
}

ssize_t vfs::devfs::write(node *file, void *buf, size_t len, off_t offset) {
    auto private_data = (dev_priv *) file->private_data;
    if (!private_data) return -ENOENT;

    devfs::device *device = private_data->dev;
    if (!device) return -ENOENT;

    if (device->is_blockdev) {
        if (private_data->part >= 0) {
            auto part = device->blockdev.part_list.data()[private_data->part];
            return device->write(buf, len, offset + (part.begin * device->blockdev.block_size));
        }
    }

    return device->write(buf, len, offset);
}

ssize_t vfs::devfs::ioctl(node *file, size_t req, void *buf) {
    auto device = (devfs::device *) file->private_data;
    if (!device) {
        return -ENOENT;
    }

    switch (req) {
        case BLKRRPART:
            if (!device->is_blockdev) return -1;
            device->blockdev.part_list.clear();
            return part::probe(device);
        default:
            return device->ioctl(req, buf);
    }
}

void *vfs::devfs::mmap(node *file, void *addr, size_t len, off_t offset) {
    auto device = (devfs::device *) file->private_data;
    if (!device) {
        return nullptr;
    }

    return device->mmap(file, addr, len, offset);
}

ssize_t vfs::devfs::mkdir(node *dst, frg::string_view name, int64_t flags) {
    node *new_dir = frg::construct<vfs::node>(memory::mm::heap, this, name, dst, flags, node::type::DIRECTORY);
    dst->children.push_back(new_dir);

    return 0;
}