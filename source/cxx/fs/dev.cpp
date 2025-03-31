#include "driver/dtable.hpp"
#include "driver/part.hpp"
#include "frg/string.hpp"
#include "fs/cache.hpp"
#include "mm/common.hpp"
#include "mm/mm.hpp"
#include "smarter/smarter.hpp"
#include "util/log/log.hpp"
#include "util/log/panic.hpp"
#include "util/misc.hpp"
#include "util/types.hpp"
#include <cstddef>
#include <frg/allocation.hpp>
#include <fs/vfs.hpp>
#include <fs/dev.hpp>

static log::subsystem logger = log::make_subsystem("DEV");
vfs::devfs::rootbus *vfs::devfs::mainbus = nullptr;

void vfs::devfs::rootbus::attach(ssize_t major, void *aux) {
    switch(major) {
        case dtable::majors::PCI: {
            pcibus *pci_bus = frg::construct<pcibus>(memory::mm::heap, this, 0);

            bus_devices.push_back(pci_bus);
            pci_bus->minor = bus_devices.size() - 1;
            pci_bus->enumerate();
        }

        default: {
            auto matcher = dtable::lookup_by_major(major);
            if (matcher) {
                auto device = matcher->match(this, nullptr);
                if (device) {
                    matcher->attach(this, device, nullptr);

                    bus_devices.push_back(device);
                    vfs::devfs::append_device(device, device->major);
                }
            }
        }
    }
}

void vfs::devfs::rootbus::enumerate() {
    attach(dtable::majors::PCI, nullptr);
}

void vfs::devfs::init() {
    vfs::mkdir(nullptr, "/dev", 0, DEFAULT_MODE, 0, 0);
    vfs::mount("/", "/dev", fslist::DEVFS, mflags::NOSRC);

    vfs::mkdir(nullptr, "/dev/pts", 0, DEFAULT_MODE, 0, 0);
    kmsg(logger, "Initial devfs mounted.");
}

void vfs::devfs::probe() {
    mainbus = frg::construct<rootbus>(memory::mm::heap);
    mainbus->enumerate();
}

void vfs::devfs::append_device(device *dev, ssize_t major) {
    size_t idx = 0;
    devfs::matcher *matcher = dtable::lookup_by_major(major);
    if (!matcher) return;

    if (device_map.contains(major)) {
        if (matcher->single && device_map[major].list.size() >= 1) return;

        device_map[major].list.push(dev);
        idx = device_map[major].last_index++;
        dev->minor = idx;
    } else {
        device_map[major] = device_list{};
        device_map[major].list.push(dev);

        idx = device_map[major].last_index++;
        dev->minor = idx;
    }

    switch(dev->cls) {
        case device_class::CHARDEV:
        case device_class::BLOCKDEV:
        case device_class::OTHER: {
            if (!matcher->has_file) break;
        
            vfs::path device_path{};
            if (matcher->subdir) {
                device_path += matcher->subdir;
                device_path += "/";
            }
        
            if (matcher->base_name) device_path += matcher->base_name;

            if (!matcher->single) {
                if (matcher->alpha_names) {
                    device_path += alpha_lower[matcher->start_index + idx];   
                } else {
                    device_path += matcher->start_index + idx + 48;
                }    
            }
        
            auto device_node = vfs::make_recursive(vfs::device_fs()->root, device_path, dev->cls == device_class::BLOCKDEV ? node::type::BLOCKDEV : node::type::CHARDEV, DEFAULT_MODE);
            if (!device_node) {
                panic("[DEVFS]: Unable to make device: %s", device_path.data());
            }
        
            auto private_data = smarter::allocate_shared<dev_priv>(memory::mm::heap);
            private_data->dev = dev;
            private_data->part = -1;
            device_node->as_data(private_data);
        
            devfs::filedev *filedev = (devfs::filedev *) dev;
            filedev->file = device_node;
        
            if (dev->cls == device_class::BLOCKDEV) {
                auto block_device = (blockdev *) dev;
                block_device->disk_cache = cache::create_cache(block_device);

                part::probe(block_device);
            }

            break;
        }

        case device_class::BUS: {
            ((busdev *) dev)->enumerate();
            break;
        }
    }
}

void vfs::devfs::remove_device(device *dev, ssize_t major) {
    if (device_map.contains(major)) {
        for (size_t i = 0; i < device_map[major].list.size(); i++) {
            if (device_map[major].list[i] == dev) {
                device_map[major].list.erase(dev);
                if (dev->minor < device_map[major].last_index) {
                    device_map[major].last_index = dev->minor;
                }

                return;
            }
        }
    } else {
        panic("Attempted to remove non-attached device");
    }
}

weak_ptr<vfs::node> vfs::devfs::lookup(shared_ptr<node> parent, frg::string_view name) {
    auto node = parent->find_child(name);
    if (!node) return {};

    auto private_data = node->data_as<dev_priv>();
    if (!private_data) return {};

    devfs::device *device = private_data->dev;
    if (!device) return {};

    return node;
}

ssize_t vfs::devfs::on_open(shared_ptr<fd> fd, ssize_t flags) {
    auto private_data = fd->desc->node->data_as<dev_priv>();
    if (!private_data) return -ENOENT;

    devfs::filedev *device = (filedev *) private_data->dev;
    if (!device) return -ENOENT;

    return device->on_open(fd, flags);
}

ssize_t vfs::devfs::on_close(shared_ptr<fd>fd, ssize_t flags) {
    auto private_data = fd->desc->node->data_as<dev_priv>();
    if (!private_data) return -ENOENT;

    devfs::filedev *device = (filedev *) private_data->dev;
    if (!device) return -ENOENT;

    return device->on_close(fd, flags);
}

ssize_t vfs::devfs::read(shared_ptr<node> file, void *buf, size_t len, off_t offset) {
    auto private_data = file->data_as<dev_priv>();
    if (!private_data) return -ENOENT;

    devfs::filedev *device = (filedev *) private_data->dev;
    if (!device) return -ENOENT;

    if (device->cls == device_class::BLOCKDEV) {
        devfs::blockdev *blockdev = (devfs::blockdev *) device;

        if (private_data->part >= 0) {
            auto part = blockdev->part_list.data()[private_data->part];
            offset = offset + (part.begin * blockdev->block_size);
        }

        auto cache = blockdev->disk_cache;
        auto res = cache->request_io(buf, offset, len, false);
        if (res < 0) {
            return -EIO;
        }

        return len;
    }

    return device->read(buf, len, offset);
}

ssize_t vfs::devfs::write(shared_ptr<node> file, void *buf, size_t len, off_t offset) {
    auto private_data = file->data_as<dev_priv>();
    if (!private_data) return -ENOENT;

    devfs::filedev *device = (filedev *) private_data->dev;
    if (!device) return -ENOENT;

    if (device->cls == device_class::BLOCKDEV) {
        devfs::blockdev *blockdev = (devfs::blockdev *) device;

        if (private_data->part >= 0) {
            auto part = blockdev->part_list.data()[private_data->part];
            offset = offset + (part.begin * blockdev->block_size);
        }

        auto cache = blockdev->disk_cache;
        auto res = cache->request_io(buf, offset, len, true);
        if (res < 0) {
            return -EIO;
        }

        return len;
    }

    return device->write(buf, len, offset);
}

ssize_t vfs::devfs::ioctl(shared_ptr<node> file, size_t req, void *buf) {
    auto private_data = file->data_as<dev_priv>();
    if (!private_data) return -ENOENT;

    devfs::filedev *device = (filedev *) private_data->dev;
    if (!device) return -ENOENT;

    switch (req) {
        default:
            return device->ioctl(req, buf);
    }
}

void *vfs::devfs::mmap(shared_ptr<node> file, void *addr, size_t len, off_t offset) {
    auto private_data = file->data_as<dev_priv>();
    if (!private_data) return nullptr;

    devfs::filedev *device = (filedev *) private_data->dev;
    if (!device) return nullptr;

    if (device->cls == device_class::BLOCKDEV) {
        devfs::blockdev *blockdev = (devfs::blockdev *) device;
        if ((offset + len) % blockdev->block_size != 0) return nullptr;

        if (private_data->part >= 0) {
            auto part = blockdev->part_list.data()[private_data->part];
            return nullptr;
            //return device->mmap(file, addr, len, offset + (part.begin * blockdev->block_size));
        }
    }

    return nullptr;
    //return device->mmap(file, addr, len, offset);
}

ssize_t vfs::devfs::poll(shared_ptr<node> file, sched::thread *thread) {
    auto private_data = file->data_as<dev_priv>();
    if (!private_data) return -ENOENT;

    devfs::filedev *device = (filedev *) private_data->dev;
    if (!device) return -ENOENT;

    return device->poll(thread);
}

ssize_t vfs::devfs::mkdir(shared_ptr<node> dst, frg::string_view name, int64_t flags, mode_t mode,
    uid_t uid, gid_t gid) {
    auto new_dir = smarter::allocate_shared<vfs::node>(memory::mm::heap, selfPtr, name, dst, flags, node::type::DIRECTORY);

    new_dir->meta->st_uid = uid;
    new_dir->meta->st_gid = gid;
    new_dir->meta->st_mode = S_IFDIR | mode;

    dst->children.push_back(new_dir);
    return 0;
}