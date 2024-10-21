#include <cstddef>
#include <frg/allocation.hpp>
#include <fs/vfs.hpp>
#include <fs/rootfs.hpp>
#include <mm/mm.hpp>
#include <mm/pmm.hpp>
#include <util/string.hpp>

vfs::node *vfs::rootfs::lookup(const pathlist& filepath, frg::string_view path, int64_t flags) {
    if (nodenames.contains(path)) {
        return nodenames[path];
    } else {
        return nullptr;
    }
}

vfs::ssize_t vfs::rootfs::write(node *file, void *buf, ssize_t len, ssize_t offset) {
    auto storage = file_storage[file->get_path()];
    if (storage.length < len + offset) {
        storage.buf = krealloc(storage.buf, len + offset);
    }

    memcpy((char *) storage.buf + offset, buf, len);
    return len;
}

vfs::ssize_t vfs::rootfs::read(node *file, void *buf, ssize_t len, ssize_t offset) {
    auto storage = file_storage[file->get_path()];
    if (storage.length > len + offset) {
        memcpy(buf, (char *) storage.buf + offset, len);
        return len;
    } else if (storage.length > offset && storage.length < len + offset) {
        memcpy(buf, (char *) storage.buf + offset, storage.length - offset);
        return storage.length - offset;
    } else {
        return 0;
    }
}

vfs::ssize_t vfs::rootfs::create(path name, node *parent, node *nnode, int64_t type, int64_t flags) {
    auto storage = rootfs::storage{};
    storage.buf = kmalloc(memory::common::page_size);
    storage.length = memory::common::page_size;

    file_storage[nnode->get_path()] = storage;
    return 0;
}

vfs::ssize_t vfs::rootfs::mkdir(const pathlist& dirpath, int64_t flags) {
    return 0;
}

vfs::ssize_t vfs::rootfs::remove(node *dest) {
    auto storage = file_storage[dest->get_path()];
    kfree(storage.buf);
    storage.buf = nullptr;

    file_storage.remove(dest->get_path());
    return 0;
}

vfs::ssize_t vfs::rootfs::lsdir(node *dir, pathlist& names) {
    for (auto& [_, child] : *root->get_children()) {
        names.push_back(child->get_name());
    }

    return 0;
}