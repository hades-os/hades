#include "smarter/smarter.hpp"
#include "util/types.hpp"
#include <cstddef>
#include <frg/allocation.hpp>
#include <fs/vfs.hpp>
#include <fs/rootfs.hpp>
#include <mm/mm.hpp>
#include <mm/pmm.hpp>
#include <util/string.hpp>

weak_ptr<vfs::node> vfs::rootfs::lookup(shared_ptr<node> parent, frg::string_view name) {
    if (parent->find_child(name)) {
        return parent->find_child(name);
    } else {
        return {};
    }
}

ssize_t vfs::rootfs::write(shared_ptr<node> file, void *buf, size_t len, off_t offset) {
    auto storage = file->data_as<rootfs::storage>();
    if (storage->length < len + offset) {
        void *old = storage->buf;
        storage->buf = kmalloc(len + offset);
        memcpy(storage->buf, old, storage->length);
        storage->length = len + offset;
    }

    memcpy((char *) storage->buf + offset, buf, len);
    return len;
}

ssize_t vfs::rootfs::read(shared_ptr<node> file, void *buf, size_t len, off_t offset) {
    auto storage = file->data_as<rootfs::storage>();
    if (storage->length > len + offset) {
        memcpy(buf, (char *) storage->buf + offset, len);
        return len;
    } else if (storage->length > offset && storage->length < len + offset) {
        memcpy(buf, (char *) storage->buf + offset, storage->length - offset);
        return storage->length - offset;
    } else {
        return 0;
    }
}

ssize_t vfs::rootfs::create(shared_ptr<node> dst, path name, int64_t type, int64_t flags, mode_t mode,
    uid_t uid, gid_t gid) {
    auto storage = smarter::allocate_shared<rootfs::storage>(memory::mm::heap);
    storage->buf = kmalloc(memory::page_size);
    storage->length = memory::page_size;

    auto new_file = smarter::allocate_shared<vfs::node>(memory::mm::heap, self, name, dst, flags, type);

    new_file->meta->st_uid = uid;
    new_file->meta->st_gid = gid;
    new_file->meta->st_mode = mode | S_IFREG;

    new_file->as_data(storage);
    dst->children.push_back(new_file);

    return 0;
}

ssize_t vfs::rootfs::mkdir(shared_ptr<node> dst, frg::string_view name, int64_t flags, mode_t mode,
    uid_t uid, gid_t gid) {
    auto new_dir = smarter::allocate_shared<vfs::node>(memory::mm::heap, self, name, dst, flags, node::type::DIRECTORY);

    new_dir->meta->st_uid = uid;
    new_dir->meta->st_gid = gid;
    new_dir->meta->st_mode = mode | S_IFDIR;

    dst->children.push_back(new_dir);

    return 0;
}

ssize_t vfs::rootfs::remove(shared_ptr<node> dest) {
    auto storage = dest->data_as<rootfs::storage>();
    kfree(storage->buf);

    return 0;
}