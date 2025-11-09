#include "mm/common.hpp"
#include "sys/sched/mail.hpp"
#include <sys/sched/sched.hpp>
#include "util/lock.hpp"
#include <cstdint>
#include <frg/allocation.hpp>
#include <mm/mm.hpp>
#include <cstddef>
#include <frg/string.hpp>
#include <fs/dev.hpp>
#include <fs/fat.hpp>
#include <fs/rootfs.hpp>
#include <fs/vfs.hpp>
#include <mm/mm.hpp>
#include <util/log/log.hpp>
#include <util/string.hpp>

vfs::node *tree_root = nullptr;
vfs::pathmap<vfs::filesystem *> mounts{vfs::path_hasher()};

void vfs::init() {
    mount("/", "/", fslist::ROOTFS, nullptr, mflags::NOSRC);
    kmsg("[VFS] Initialized");
}

vfs::filesystem *vfs::resolve_fs(frg::string_view path) {
    if (path == '/') {
        return mounts["/"];
    }

    vfs::filesystem *fs = nullptr;

    auto path_view = path;
    auto n_seperators = path_view.count('/') + 1;

    for (size_t i = 0; i < n_seperators && !fs; i++) {
        fs = mounts[path_view];
        if (path_view.find_last('/') != size_t(-1))
            path_view = path_view.sub_string(0, path_view.find_last('/'));
    }

    if (!fs) {
        return mounts["/"];
    }

    return fs;
}

static frg::string_view strip_leading(frg::string_view path) {
    if (path[0] == '/' && path != '/') {
        return path.sub_string(1);
    }

    return path;
}

vfs::node *vfs::resolve_process(frg::string_view path, sched::process *proc) {
    return resolve_at(path, proc->cwd);
}

vfs::node *vfs::resolve_at(frg::string_view path, node *base) {
    if (path == '/') {
        return tree_root;
    }

    auto adjusted_path = strip_leading(path);
    auto split_path = vfs::split_path(adjusted_path);
    auto absolute_list = pathlist();
    auto absolute_path = vfs::path();

    node *current;
    if (path[0] == '/' || base == nullptr) {
        current = tree_root;
    } else {
        current = base;
        auto adjusted_base_path = strip_leading(base->get_path());
        absolute_path += adjusted_base_path;
    }

    for (auto component: split_path) {
        if (component == ".") {
            continue;
        } else if (component == "..") {
            if (current->get_parent() == nullptr) {
                current = tree_root;
                continue;
            }

            current = current->get_parent();
        } else {
            if (!absolute_list.empty()) {
                absolute_list.push(vfs::path("/") + current->get_name());
                absolute_path += vfs::path("/") + current->get_name();
            } else {
                absolute_list.push(current->get_name());
                absolute_path += current->get_name();
            }
        }
    }

    auto adjusted_absolute_path = strip_leading(absolute_path);
    return resolve_fs(adjusted_absolute_path)->lookup(absolute_list, adjusted_absolute_path, 0);
}

vfs::node *vfs::resolve_abs(frg::string_view path) {
    if (path == '/') {
        return tree_root;
    }

    auto adjusted_path = strip_leading(path);
    auto split_path = vfs::split_path(adjusted_path);

    return resolve_fs(adjusted_path)->lookup(split_path, adjusted_path, 0);
}

vfs::ssize_t vfs::lseek(vfs::fd *fd, size_t off, size_t whence) {
    auto desc = fd->desc;
    if (!desc->node) return -error::NOSYS;
    if (desc->node->get_type() == node::type::DIRECTORY) {
        return -error::ISDIR;
    }

    switch (whence) {
        case sflags::CUR:
            desc->pos = desc->pos + off;
            return desc->pos;;
        case sflags::SET:
            desc->pos = off;
            return desc->pos;
        case sflags::END:
            desc->pos = desc->node->stat()->st_size;
            return desc->pos;
        default:
            return -error::INVAL;
    }

    return 0;
}

vfs::ssize_t vfs::read(vfs::fd *fd, void *buf, vfs::ssize_t len) {
    auto desc = fd->desc;
    if (!desc->node) {
        if (desc->pos > desc->info->st_size) {
            while (__atomic_load_n(&desc->pipe->data_written, __ATOMIC_RELAXED) == 0);
            __atomic_clear(&desc->pipe->data_written, __ATOMIC_RELAXED);
        }

        if (desc->pos + len > desc->info->st_size) {
            len = desc->info->st_size - desc->pos;
        }

        memcpy(buf, (char *) desc->pipe->buf + desc->pos, len);
        desc->pos += len;
        return len;
    }

    if (desc->node->get_type() == node::type::DIRECTORY) {
        return -error::ISDIR;
    }

    auto res = desc->node->get_fs()->read(desc->node, buf, len, desc->pos);
    if (res >= 0) desc->pos += res;
    return res;
}

vfs::ssize_t vfs::write(vfs::fd *fd, void *buf, ssize_t len) {
    auto desc = fd->desc;
    if (!desc->node) {
        if (desc->pos >= memory::common::page_size) {
            return -error::INVAL;
        }

        if (desc->pos + len > memory::common::page_size) {
            len = desc->info->st_size - desc->pos;
        }

        if (desc->pos + len > desc->info->st_size) {
            desc->info->st_size = desc->pos + len - desc->info->st_size;
        }

        memcpy((char *) desc->pipe->buf + desc->pos, buf, len);
        desc->pos += len;

        if (desc->pos > desc->info->st_size) {
            desc->pipe->data_written = true;            
        }

        return len;
    }

    if (desc->node->get_type() == node::type::DIRECTORY) {
        return -error::ISDIR;
    }

    auto res = desc->node->get_fs()->write(desc->node, buf, len, desc->pos);
    if (res >= 0) desc->pos += res;
    return res;
}

vfs::ssize_t vfs::ioctl(vfs::fd *fd, size_t req, void *buf) {
    auto desc = fd->desc;
    return desc->node->get_fs()->ioctl(desc->node, req, buf);
}

vfs::node *vfs::get_parent(frg::string_view filepath) {
    if (filepath[0] == '/' && filepath.count('/') == 1) {
        return tree_root;
    }

    auto parent_path = filepath;
    if (parent_path.find_last('/') != size_t(-1))
        parent_path = parent_path.sub_string(0, parent_path.find_last('/'));

    auto parent = resolve_abs(parent_path);
    if (!parent) {
        return nullptr;
    }

    return parent;
}

vfs::node *vfs::insert_node(frg::string_view filepath, int64_t type) {
    auto parent = get_parent(filepath);

    auto name = find_name(filepath);
    auto adjusted_filepath = strip_leading(filepath);
    auto fs = parent->get_fs();
    auto node = frg::construct<vfs::node>(memory::mm::heap, fs, name, adjusted_filepath, parent, 0, type);
    fs->nodenames[adjusted_filepath] = node;

    return node;
}

vfs::ssize_t vfs::create(frg::string_view filepath, fd_table *table, int64_t type, int64_t flags, int64_t mode) {
    auto parent = get_parent(filepath);
    if (!parent) {
        return -error::NOENT;
    }

    if (parent->get_type() != node::type::DIRECTORY) {
        return -error::INVAL;
    }

    auto name = find_name(filepath);
    auto adjusted_filepath = strip_leading(filepath);
    auto fs = parent->get_fs();
    auto node = frg::construct<vfs::node>(memory::mm::heap, fs, name, adjusted_filepath, parent, flags, type);
    fs->nodenames[adjusted_filepath] = node;
    size_t err = 0;
    if (type == node::type::DIRECTORY) {
        if ((err = fs->mkdir(split_path(filepath), flags)) != 0) {
            fs->nodenames.remove(adjusted_filepath);
            frg::destruct(memory::mm::heap, node);
            return err;
        }

        return 0;
    }

    if ((err = fs->create(name, parent, node, type, flags)) != 0) {
        fs->nodenames.remove(adjusted_filepath);
        frg::destruct(memory::mm::heap, node);
        return err;
    }

    if (flags & oflags::NOFD) {
        return -error::NOFD;
    }

    return -error::EXIST;
}

static inline vfs::node *follow_links(vfs::node *src) {
    if (!src) {
        return nullptr;
    }

    auto dst = src;
    while (dst->get_master()) {
        dst = dst->get_master();
    }

    return dst;
}

vfs::fd_table *vfs::make_table() {
    auto table = frg::construct<fd_table>(memory::mm::heap);
    table->lock = util::lock{};
    table->last_fd++;

    return table;
}

vfs::fd_table *vfs::copy_table(fd_table *table) {
    auto new_table = make_table();
    new_table->last_fd = table->last_fd;

    for (auto [fd_number, fd]: table->fd_list) {
        auto desc = fd->desc;

        auto new_desc = frg::construct<vfs::descriptor>(memory::mm::heap);
        auto new_fd = frg::construct<vfs::fd>(memory::mm::heap);

        new_desc->lock = util::lock();
        new_desc->node = desc->node;
        new_desc->pipe = desc->pipe;

        new_desc->ref = 1;
        new_desc->pos = desc->pos;
        new_desc->info = desc->info;

        new_fd->lock = util::lock();
        new_fd->desc = new_desc;
        new_fd->table = new_table;
        new_fd->fd_number = fd_number;
        new_fd->flags = fd->flags;
        new_fd->mode = fd->mode;

        new_table->fd_list[fd->fd_number] = new_fd;
    }

    return new_table;
}

void vfs::delete_table(fd_table *table) {
    for (auto [fd_number, fd]: table->fd_list) {
        auto desc = fd->desc;
        if (desc->node) desc->node->ref_count--;
        frg::destruct(memory::mm::heap, desc);
        frg::destruct(memory::mm::heap, fd);
    }

    frg::destruct(memory::mm::heap, table);
}

vfs::fd *vfs::make_fd(vfs::node *node, fd_table *table, int64_t flags, int64_t mode) {
    auto desc = frg::construct<vfs::descriptor>(memory::mm::heap);
    auto fd = frg::construct<vfs::fd>(memory::mm::heap);

    desc->lock = util::lock();
    desc->node = node;
    desc->pipe = nullptr;

    desc->ref = 1;
    desc->pos= 0;

    desc->info = nullptr;

    fd->lock = util::lock();
    fd->desc = desc;
    fd->table = table;
    fd->fd_number = table->last_fd++;
    fd->flags = flags;
    fd->mode = mode;

    if (node) {
        auto open_val = node->get_fs()->on_open(fd, flags);
        if (open_val != -error::NOSYS && open_val < 0) {
            frg::destruct(memory::mm::heap, desc);
            frg::destruct(memory::mm::heap, fd);
    
            return nullptr;
        }
    }

    table->lock.irq_acquire();
    table->fd_list[fd->fd_number] = fd;
    table->lock.irq_release();

    return fd;
}

vfs::fd *vfs::open(frg::string_view filepath, fd_table *table, int64_t flags, int64_t mode) {
    if (!table) {
        return nullptr;
    }

    auto node = follow_links(resolve_abs(filepath));
    if (!node) {
        if (flags & oflags::CREAT && table) {
            auto err = create(filepath, table, vfs::node::type::FILE, flags, mode);
            if (err <= 0) {
                return nullptr;
            }
        } else {
            return nullptr;
        }
    }

    return make_fd(node, table, flags, mode);
}

vfs::fd_pair vfs::open_pipe(fd_table *table, ssize_t flags) {
    auto read = make_fd(nullptr, table, flags, mode::RDONLY);
    auto write = make_fd(nullptr, table, flags, mode::WRONLY);

    auto pipe = frg::construct<vfs::pipe>(memory::mm::heap);
    pipe->read = read->desc;
    pipe->write = write->desc;
    pipe->len = memory::common::page_size;
    pipe->buf = kmalloc(memory::common::page_size);
    pipe->data_written = false;

    auto stat = frg::construct<node::statinfo>(memory::mm::heap);
    stat->st_size = 0;

    write->desc->pipe = pipe;
    write->desc->info = stat;
    read->desc->pipe = pipe;
    read->desc->info = stat;

    return {read, write};
}

vfs::fd *vfs::dup(vfs::fd *fd, ssize_t flags, ssize_t new_num) {
    if (fd == nullptr) {
        return nullptr;
    }

   if (fd->fd_number == new_num) {
        return fd;
    }

    fd->desc->ref++;
    auto new_fd = fd->table->fd_list[new_num];
    if (new_num >= 0) {
        close(new_fd);
    }

    new_fd = frg::construct<vfs::fd>(memory::mm::heap);
    new_fd->lock = util::lock();
    new_fd->desc = fd->desc;
    new_fd->table = fd->table;
    if (new_num >= 0) {
        new_fd->fd_number = new_num;
    } else {
        new_fd->fd_number = fd->table->last_fd++;
    }

    new_fd->flags = flags;
    new_fd->mode = fd->mode;

    fd->table->lock.irq_acquire();
    fd->table->fd_list[new_fd->fd_number] = new_fd;
    fd->table->lock.irq_release();

    return new_fd;    
}

vfs::ssize_t vfs::lstat(frg::string_view filepath, node::statinfo *buf) {
    auto node = resolve_abs(filepath);
    if (!node) {
        return -error::NOENT;
    }

    memcpy(buf, node->stat(), sizeof(node::statinfo));

    return 0;
}

vfs::ssize_t vfs::close(vfs::fd *fd) {
    auto desc = fd->desc;
    desc->ref--;

    if (desc->node) {
        desc->node->ref_count--;
        desc->node->get_fs()->on_close(fd, fd->flags);
    }

    fd->table->fd_list.remove(fd->fd_number);
    frg::destruct(memory::mm::heap, fd);
    if (desc->ref <= 0) {
        if (desc->info) frg::destruct(memory::mm::heap, desc->info);
        frg::destruct(memory::mm::heap, desc);
    }

    return 0;
}

vfs::ssize_t vfs::mkdir(frg::string_view dirpath, int64_t flags, int64_t mode) {
    auto dir = resolve_abs(dirpath);
    if (dir) {
        switch (dir->get_type()) {
            case node::type::DIRECTORY:
                return -error::ISDIR;
            default:
                return -error::EXIST;
        }
    }

    auto parent = get_parent(dirpath);
    if (!parent) {
        return -error::NOTDIR;
    }

    return create(dirpath, nullptr, node::type::DIRECTORY, flags, mode);
}

vfs::ssize_t vfs::rename(frg::string_view oldpath, frg::string_view newpath, int64_t flags) {
    auto oldview = frg::string_view(oldpath);
    auto newview = frg::string_view(newpath);

    if (newview.size() > oldview.size() && newview.sub_string(0, oldview.size()) == oldview) {
        return -error::INVAL;
    }

    auto src = resolve_abs(oldpath);
    if (!src) {
        return -error::INVAL;
    }

    auto dst = resolve_abs(newpath);
    if (dst) {
        if (dst->ref_count || src->ref_count) {
            return -error::BUSY;
        }

        switch (dst->get_type()) {
            case node::type::SOCKET:
            case node::type::BLOCKDEV:
            case node::type::CHARDEV:
                return -error::INVAL;
            default:
                break;
        }

        if (dst->get_type() == node::type::DIRECTORY && src->get_type() == node::type::DIRECTORY) {
            if (dst->get_ccount()) {
                return -error::NOTEMPTY;
            }
        }

        if ((dst->get_type() == node::type::FILE && src->get_type() == node::type::DIRECTORY) ||
            (dst->get_type() == node::type::DIRECTORY && src->get_type() == node::type::FILE)) {
                return -error::INVAL;
        }

        if (dst->get_type() == node::type::SYMLINK && src->get_type() == node::type::SYMLINK) {
            ssize_t err = 0;
            auto master = dst->get_master();
            auto fs = dst->get_fs();
            auto parent = dst->get_parent();
            auto path = dst->get_path();
            dst->clear_master();
            if ((err = dst->get_fs()->remove(dst)) != 0) {
                return err;
            }

            if ((err = fs->rename(split_path(oldpath), split_path(newpath), flags)) != 0) {
                return err;
            }

            src->set_parent(parent);
            src->set_path(path);
            src->set_master(master);

            return 0;
        }
    }

    ssize_t err = 0;
    if ((err = dst->get_fs()->rename(split_path(oldpath), split_path(newpath), flags)) != 0) {
        return err;
    }

    if (resolve_fs(newpath) != src->get_fs()) {
        return -error::XDEV;
    }

    auto parent = get_parent(newpath);
    src->set_parent(parent);
    src->set_path(newpath);

    return 0;
}

vfs::ssize_t vfs::link(frg::string_view from, frg::string_view to, bool is_symlink) {
    auto src = resolve_abs(from);
    auto dst = resolve_abs(to);

    if (!is_symlink) {
        if (!src) {
            return -error::INVAL;
        }

        if (resolve_fs(to) != src->get_fs()) {
            return -error::XDEV;
        }

        if (dst) {
            return -error::EXIST;
        }

        if (src->ref_count) {
            return -error::BUSY;
        }

        ssize_t err = 0;
        auto fs = resolve_fs(to);
        if ((err = fs->link(split_path(from), split_path(to), false)) != 0) {
            return err;
        }
    } else {
        if (dst) {
            return -error::EXIST;
        }

        ssize_t err = 0;
        auto fs = resolve_fs(to);
        if ((err = fs->link(split_path(from), split_path(to), true)) != 0) {
            return err;
        }

        dst = resolve_abs(to);
        if (src) {
            dst->set_master(src);
        } else {
            dst->set_link(to);
        }
    }

    return 0;
}

vfs::ssize_t vfs::unlink(frg::string_view filepath) {
    auto dst = resolve_abs(filepath);
    if (!dst) {
        return -error::INVAL;
    }

    if (dst->ref_count) {
        return -error::BUSY;
    }

    if (dst->get_type() == node::type::DIRECTORY) {
        return -error::ISDIR;
    }

    if (dst->get_master() || dst->get_link()) {
        ssize_t err = 0;
        if ((err = dst->get_fs()->remove(dst)) != 0) {
            return -error::IO;
        }

        dst->clear_master();
        dst->clear_link();
        dst->get_fs()->nodenames.remove(filepath);
        frg::destruct(memory::mm::heap, dst);

        return 0;
    }

    ssize_t err = 0;
    if ((err = dst->get_fs()->remove(dst)) != 0) {
        return err;
    }

    dst->get_fs()->nodenames.remove(filepath);
    frg::destruct(memory::mm::heap, dst);

    return 0;
}

vfs::ssize_t vfs::in_use(frg::string_view filepath) {
    auto dst = resolve_abs(filepath);
    if (!dst) {
        return -error::INVAL;
    }

    if (dst->ref_count) {
        return -error::BUSY;
    }

    return 0;
}

vfs::ssize_t vfs::rmdir(frg::string_view dirpath) {
    auto dir = resolve_abs(dirpath);
    if (!dir) {
        return -error::NOENT;
    }

    if (dir->ref_count) {
        return -error::BUSY;
    }

    if (dir->get_ccount()) {
        return -error::NOTEMPTY;
    }

    if (dir->get_type() != node::type::DIRECTORY) {
        return -error::NOTDIR;
    }

    ssize_t err = 0;
    if ((err = dir->get_fs()->rmdir(dir)) != 0) {
        return err;
    }

    frg::destruct(memory::mm::heap, dir);
    return 0;
}

vfs::pathlist vfs::lsdir(frg::string_view dirpath) {
    pathlist names{};
    auto dir = resolve_abs(dirpath);
    auto fs = resolve_fs(dirpath);

    if (!dir || !fs) {
        return names;
    }

    if (dir->get_type() != node::type::DIRECTORY) {
        return names;
    }

    fs->lsdir(dir, names);
    return names;
}

vfs::ssize_t vfs::mount(frg::string_view srcpath, frg::string_view dstpath, ssize_t fstype, optlist *opts, int64_t flags) {
    auto *src = resolve_abs(srcpath);
    auto *dst = resolve_abs(dstpath);

    switch (flags) {
        case (mflags::NOSRC | mflags::NODST):
        case mflags::NODST:
        case mflags::NOSRC: {
            switch (fstype) {
                case fslist::ROOTFS: {
                    if (tree_root) {
                        return -error::NOTEMPTY;
                    }

                    auto *fs = frg::construct<rootfs>(memory::mm::heap);
                    auto *root = frg::construct<node>(memory::mm::heap, fs, "/", "/", nullptr, 0, node::type::DIRECTORY);
                    fs->init_as_root(root);
                    tree_root = root;
                    mounts["/"] = fs;
                    break;
                }

                case fslist::DEVFS: {
                    if (!dst) {
                        return -error::INVAL;
                    }

                    if (dst->get_type() != node::type::DIRECTORY) {
                        return -error::INVAL;
                    }

                    if (dst->get_ccount() > 0) {
                        return -error::INVAL;
                    }

                    auto fs = frg::construct<devfs>(memory::mm::heap);
                    fs->init_fs(dst, nullptr);
                    dst->set_fs(fs);
                    mounts[strip_leading(dstpath)] = fs;
                    break;
                }

                default:
                    return -error::INVAL;
            }
            break;
        }

        case mflags::OVERLAY: {
            switch (fstype) {
                case fslist::FAT: {
                    if (!dst || !src) {
                        kmsg("invalid src or dst");

                        return -error::INVAL;
                    }

                    if (dst->get_type() != node::type::DIRECTORY || src->get_type() != node::type::BLOCKDEV) {
                        kmsg("invalid source device or dest; dst: ", dst->get_type(), ", source: ", src->get_type());

                        return -error::INVAL;
                    }

                    auto fs = frg::construct<fatfs>(memory::mm::heap);
                    fs->init_fs(dst, src);
                    dst->set_fs(fs);
                    mounts[strip_leading(dstpath)] = fs;
                    break;
                }

                default:
                    return -error::INVAL;
            }

            break;
        }

    }

    return 0;
}

vfs::ssize_t vfs::umount(node *dst) {
    auto *fs = dst->get_fs();
    if (!dst) {
        return -error::INVAL;
    }

    if (!fs) {
        return -error::INVAL;
    }

    if (dst->ref_count) {
        return -error::BUSY;
    }

    frg::destruct(memory::mm::heap, fs);
    return 0;
}