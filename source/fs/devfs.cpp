#include "driver/part.hpp"
#include "frg/string.hpp"
#include "frg/tuple.hpp"
#include "util/log/log.hpp"
#include <cstddef>
#include <frg/allocation.hpp>
#include <fs/vfs.hpp>
#include <fs/devfs.hpp>

void vfs::devfs::init() {
    vfs::mgr()->mkdir("/dev", 0);
    vfs::mgr()->mount("/", "/dev", fslist::DEVFS, nullptr, mflags::NOSRC);
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

static vfs::ssize_t check_spec(char *name) {
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
    auto spec_pos = check_spec(num_begin);

    if (num_begin == name.end() - 1) {
        return frg::string_view{name}.sub_string(0, name.size() - 1);
    } else if (spec_pos != -1) {
        return frg::string_view{name}.sub_string(0, split_idx + spec_pos);
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
    auto spec_pos = check_spec(num_begin);

    if (num_begin == name.end() - 1) {
        return num_begin[0] - '0';
    } else if (spec_pos != -1) {
        auto num = frg::string_view{name}.sub_string(split_idx + spec_pos + 1);
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
        if (vfs::mgr()->in_use(n->get_path())) {
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
        vfs::mgr()->unlink(n->get_path());
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

vfs::ssize_t vfs::devfs::remove(node *dest) {
    return 0;
}

vfs::node *vfs::devfs::lookup(const pathlist &filepath, vfs::path path, int64_t flags) {
    if (nodenames.contains(path)) {
        return nodenames[path];
    }

    auto name_sec = extract_name(filepath[filepath.size() - 1]);
    auto part_sec = extract_part(filepath[filepath.size() - 1]);

    auto device = find(name_sec.data());
    if (!device) {
        return nullptr;
    }

    if (part_sec > (ssize_t) device->part_list.size()) {
        return nullptr;
    }

    vfs::mgr()->create(path, node::type::FILE, 0xCAFEBABE | oflags::DYNAMIC);
    return nodenames[path];
}

vfs::ssize_t vfs::devfs::create(path name, node *parent, node *nnode, int64_t type, int64_t flags) {
    if (!(flags & 0xCAFEBABE)) {
        return -error::INVAL;
    }

    return 0;
}

vfs::ssize_t vfs::devfs::read(node *file, void *buf, ssize_t len, ssize_t offset) {
    auto name_sec = extract_name(file->get_name());
    auto device = device_map[name_sec];
    if (!device) {
        return -error::NOENT;
    }

    auto part_sec = extract_part(file->get_name());
    if (part_sec != -1) {
        auto part = device->part_list.data()[part_sec];
        if (device->lmode) {
            return device->read(buf, len, (offset * device->block_size) + (part.begin * device->block_size));        
        }
        return device->read(buf, len, offset + (part.begin * device->block_size));        
    }

    return device->read(buf, len, offset);
}

vfs::ssize_t vfs::devfs::write(node *file, void *buf, ssize_t len, ssize_t offset) {
    auto name_sec = extract_name(file->get_name());
    auto device = device_map[name_sec];    
    if (!device) {
        return -error::NOENT;
    }

    auto part_sec = extract_part(file->get_name());
    if (part_sec != -1) {
        auto part = device->part_list.data()[part_sec];
        if (device->lmode) {
            return device->write(buf, len, (offset * device->block_size) + (part.begin * device->block_size)); 
        }
        return device->write(buf, len, offset + (part.begin * device->block_size));        
    }

    return device->write(buf, len, offset);
}

vfs::ssize_t vfs::devfs::ioctl(node *file, size_t req, void *buf) {
    auto name_sec = extract_name(file->get_name());
    auto device = device_map[name_sec];    
    if (!device) {
        return -error::NOENT;
    }

    switch (req) {
        case BLKRRPART:
            device->part_list.clear(); 
            return part::probe(device);
        case BLKLMODE:
            device->lmode = !device->lmode;
            return 0;
        default:
            return device->ioctl(req, buf);
    }
}

vfs::ssize_t vfs::devfs::lsdir(node *dir, pathlist& names) {
    for (auto& [name, _] : device_map) {
        names.push_back(name);
    }

    return 0;
}