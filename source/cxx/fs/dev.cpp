#include "driver/part.hpp"
#include "frg/string.hpp"
#include "frg/tuple.hpp"
#include "sys/smp.hpp"
#include "util/log/log.hpp"
#include <cstddef>
#include <frg/allocation.hpp>
#include <fs/vfs.hpp>
#include <fs/dev.hpp>

void vfs::devfs::init() {
    vfs::mkdir("/dev", 0, mode::RDWR);
    vfs::mount("/", "/dev", fslist::DEVFS, nullptr, mflags::NOSRC);

    vfs::mkdir("/dev/pts", 0, mode::RDWR);
    kmsg("[VFS] Initial devfs mounted.");
}

static bool isdigit(char c) {
    if (c >= '0' && c <= '9') {
        return true;
    }
    
    return false;
}

static size_t atoi(const char *str) {
    size_t res = 0;
    for (size_t i = 0; str[i] != '\0'; i++) {
        res = res * 10 + str[i] - '0';
    }

    return res;
}

static vfs::ssize_t find_part(char *name) {
    for (int i = 1; name[i] != '\0'; i++) {
        if (name[i] == 'p') {
            return i;
        }
    }

    return -1;
}

static vfs::path extract_name(vfs::path name) {
    auto split_idx = 0;
    for (size_t i = 0; i < name.size(); i++) {
        if (isdigit(name.begin()[i])) {
            split_idx = i;
            break;
        }
    }

    if (split_idx == 0) {
        return name;
    }

    auto num_begin = name.begin() + split_idx;
    auto part_pos = find_part(num_begin);

    if (num_begin == name.end() - 1) {
        return frg::string_view{name}.sub_string(0, name.size() - 1);
    } else if (part_pos != -1) {
        return frg::string_view{name}.sub_string(0, split_idx + part_pos);
    } else {
        return frg::string_view{name}.sub_string(0, split_idx);
    }
}

static vfs::ssize_t extract_part(vfs::path name) {
    auto split_idx = 0;
    for (size_t i = 0; i < name.size(); i++) {
        if (isdigit(name.begin()[i])) {
            split_idx = i;
            break;
        }
    }

    if (split_idx == 0) {
        return -1;
    }

    auto num_begin = name.begin() + split_idx;
    auto part_pos = find_part(num_begin);

    if (num_begin == name.end() - 1) {
        return num_begin[0] - '0';
    } else if (part_pos != -1) {
        auto num = frg::string_view{name}.sub_string(split_idx + part_pos + 1);
        return atoi(num.data());
    } else {
        auto num = frg::string_view{name}.sub_string(split_idx);
        return atoi(num.data());
    }
}

void vfs::devfs::add(device *dev) {
    device_map[dev->name] = dev;
}

bool in_use(vfs::path name) {
    auto name_sec = extract_name(name);

    if (!vfs::devfs::device_map.contains(name_sec)) {
        return false;
    }

    for (auto n : vfs::devfs::node_map[name]) {
        if (vfs::in_use(n->get_path())) {
            return true;
        }
    }

    return false;
}

void vfs::devfs::rm(vfs::path name) {
    auto name_sec = extract_name(name);
    auto part_sec = extract_part(name);
    if (!device_map.contains(name_sec)) {
        return;
    }
    
    if (part_sec == -1) {
        device_map.remove(name_sec);
    }

    for (auto n : node_map[name]) {
        vfs::unlink(n->get_path());
    }
}

void vfs::devfs::rm(vfs::ssize_t major, ssize_t minor) {
    auto device = find(major, minor);
    if (!device) {
        return;
    }
    
    if (in_use(device->name)) {
        return;
    } 

    rm(device->name);    
}

vfs::devfs::device *vfs::devfs::find(ssize_t major, ssize_t minor) {
    for (auto& [_, d] : device_map) {
        if (d->major == major && d->minor == minor) {
            return d;
        }
    }

    return nullptr;
}

vfs::devfs::device *vfs::devfs::find(vfs::path name) {
    for (auto& [_, d] : device_map) {
        if (d->name.eq(name)) {
            return d;
        }
    }

    return nullptr;
}

vfs::devfs::device *vfs::devfs::find(vfs::node *node) {
    return find(node->get_name());
}

vfs::node *vfs::devfs::lookup(const pathlist &filepath, frg::string_view path, int64_t flags) {
    if (nodenames.contains(path)) {
        return nodenames[path];
    }

    auto name_sec = extract_name(filepath[filepath.size() - 1]);
    auto device = device_map[filepath[filepath.size() - 1]];
    if (!device) {
        device = device_map[name_sec.data()];
        if (!device || !device->resolveable) {
            return nullptr;
        }
    }
    
    auto part_sec = extract_part(filepath[filepath.size() - 1]);
    if (device->blockdev) {
        if (part_sec > (ssize_t) device->block.part_list.size() - 1) {
            return nullptr;
        }
    }

    vfs::insert_node(path, device->blockdev ? node::type::BLOCKDEV : node::type::CHARDEV);
    return nodenames[path];
}

vfs::ssize_t vfs::devfs::on_open(vfs::fd *fd, ssize_t flags) {
    auto name_sec = extract_name(fd->desc->node->get_name());
    auto device = device_map[fd->desc->node->get_name()];
    if (!device) {
        device = device_map[name_sec];
        if (!device) {
            return -error::NOENT;
        }
    }

    return device->on_open(fd, flags);
}

vfs::ssize_t vfs::devfs::on_close(vfs::fd *fd, ssize_t flags) {
    auto name_sec = extract_name(fd->desc->node->get_name());
    auto device = device_map[fd->desc->node->get_name()];
    if (!device) {
        device = device_map[name_sec];
        if (!device) {
            return -error::NOENT;
        }
    }

    return device->on_close(fd, flags);
}

bool vfs::devfs::request_io(node *file, device::block_zone *zones, size_t num_zones, bool rw, bool all_success) {
    auto name_sec = extract_name(file->get_name());
    auto device = device_map[name_sec];
    if (!device) {
        return false;
    }

    auto part_sec = extract_part(file->get_name()); 
    if (!device->blockdev) {
        return false;
    }

    if (part_sec != -1) {
        auto part = device->block.part_list.data()[part_sec];
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


vfs::ssize_t vfs::devfs::read(node *file, void *buf, ssize_t len, ssize_t offset) {
    auto name_sec = extract_name(file->get_name());
    auto device = device_map[file->get_name()];
    if (!device) {
        device = device_map[name_sec];
        if (!device) {
            return -error::NOENT;
        }
    }

    auto part_sec = extract_part(file->get_name());
    if (device->blockdev) {
        if (part_sec != -1) {
            auto part = device->block.part_list.data()[part_sec];
            if (device->block.lmode) {
                return device->read(buf, len, (offset * device->block.block_size) + (part.begin * device->block.block_size));        
            }
            return device->read(buf, len, offset + (part.begin * device->block.block_size));        
        }
    }

    return device->read(buf, len, offset);
}

vfs::ssize_t vfs::devfs::write(node *file, void *buf, ssize_t len, ssize_t offset) {
    auto name_sec = extract_name(file->get_name());
    auto device = device_map[file->get_name()];
    if (!device) {
        device = device_map[name_sec];
        if (!device) {
            return -error::NOENT;
        }
    }

    auto part_sec = extract_part(file->get_name());
    if (device->blockdev) {
        if (part_sec != -1) {
            auto part = device->block.part_list.data()[part_sec];
            if (device->block.lmode) {
                return device->write(buf, len, (offset * device->block.block_size) + (part.begin * device->block.block_size));        
            }
            return device->write(buf, len, offset + (part.begin * device->block.block_size));        
        }
    }

    return device->write(buf, len, offset);
}

vfs::ssize_t vfs::devfs::ioctl(node *file, size_t req, void *buf) {
    auto name_sec = extract_name(file->get_name());
    auto device = device_map[file->get_name()];
    if (!device) {
        device = device_map[name_sec];
        if (!device) {
            return -error::NOENT;
        }
    }

    switch (req) {
        case BLKRRPART:
            if (!device->blockdev) return -1;
            device->block.part_list.clear(); 
            return part::probe(device);
        case BLKLMODE:
            if (!device->blockdev) return -1;
            device->block.lmode = !device->block.lmode;
            return 0;
        default:
            return device->ioctl(req, buf);
    }
}

void *vfs::devfs::mmap(node *file, void *addr, ssize_t len, ssize_t offset) {
    auto name_sec = extract_name(file->get_name());
    auto device = device_map[file->get_name()];
    if (!device) {
        device = device_map[name_sec];
        if (!device) {
            return nullptr;
        }
    }

    return device->mmap(file, addr, len, offset);
}

vfs::ssize_t vfs::devfs::mkdir(const pathlist& dirpath, int64_t flags) {
    return 0;
}

vfs::ssize_t vfs::devfs::lsdir(node *dir, pathlist& names) {
    for (auto& [name, _] : device_map) {
        names.push_back(name);
    }

    return 0;
}