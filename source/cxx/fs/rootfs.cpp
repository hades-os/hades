#include "mm/slab.hpp"
#include "smarter/smarter.hpp"
#include "util/types.hpp"
#include <cstddef>
#include <prs/construct.hpp>
#include <fs/vfs.hpp>
#include <fs/rootfs.hpp>
#include <mm/mm.hpp>
#include <mm/pmm.hpp>
#include <util/string.hpp>

weak_ptr<vfs::node> vfs::rootfs::lookup(shared_ptr<node> parent, prs::string_view name) {
    if (parent->child_by_name(name)) {
        return parent->child_by_name(name);
    } else {
        return {};
    }
}

ssize_t vfs::rootfs::write(shared_ptr<node> file, void *buf, size_t len, off_t offset) {
    auto storage = file->data_as<rootfs::storage>();
    if (storage->length < len + offset) {
        void *old = storage->buf;
        storage->buf = allocator.allocate(len + offset);
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
    auto storage = prs::allocate_shared<rootfs::storage>(prs::allocator{slab::create_resource()});
    storage->buf = allocator.allocate(memory::page_size);
    storage->length = memory::page_size;

    auto new_file = prs::allocate_shared<vfs::node>(prs::allocator{slab::create_resource()}, self, name, dst, flags, type);

    new_file->meta->st_uid = uid;
    new_file->meta->st_gid = gid;
    new_file->meta->st_mode = mode | S_IFREG;

    new_file->as_data(storage);
    dst->child_add(new_file);

    return 0;
}

ssize_t vfs::rootfs::mkdir(shared_ptr<node> dst, prs::string_view name, int64_t flags, mode_t mode,
    uid_t uid, gid_t gid) {
    auto new_dir = prs::allocate_shared<vfs::node>(prs::allocator{slab::create_resource()}, self, name, dst, flags, node::type::DIRECTORY);

    new_dir->meta->st_uid = uid;
    new_dir->meta->st_gid = gid;
    new_dir->meta->st_mode = mode | S_IFDIR;

    dst->child_add(new_dir);

    return 0;
}

ssize_t vfs::rootfs::remove(shared_ptr<node> dest) {
    auto storage = dest->data_as<rootfs::storage>();
    allocator.deallocate(storage->buf);

    return 0;
}