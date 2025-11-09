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

vfs::filesystem *vfs::manager::resolve_fs(vfs::path path) {
    if (path == "/") {
        return mounts["/"];
    }

    vfs::filesystem *fs = nullptr;
    auto path_tmp = vfs::path{path};
    auto path_view = frg::string_view{path_tmp};
    auto n_seperators = path_tmp.count('/') + 1;

    for (size_t i = 0; i < n_seperators && !fs; i++) {
        fs = mounts[path_view];
        path_tmp.resize(path_view.find_last('/'));
        path_view = path_tmp;
    }

    if (!fs) {
        return mounts["/"];
    }

    return fs;
}

static frg::string_view adjust_path(vfs::path& path) {
    if (path.startswith('/') && path != '/') {
        return frg::string_view{path}.sub_string(1);
    }

    return frg::string_view{path};
}

vfs::node *vfs::manager::resolve(vfs::path path) {
    if (path == '/') {
        return root;
    }

    auto adjusted_path = adjust_path(path);
    auto split_path = split(adjusted_path);

    return resolve_fs(adjusted_path)->lookup(split_path, adjusted_path, 0);
}

vfs::ssize_t vfs::manager::lseek(ssize_t fd, size_t off, size_t whence) {
    if (!fd_table.contains(fd)) {
        return -error::INVAL;
    }
    
    auto desc = fd_table[fd];
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

vfs::ssize_t vfs::manager::read(ssize_t fd, void *buf, vfs::ssize_t len) {
    if (!fd_table.contains(fd)) {
        return -error::INVAL;
    }
    
    auto desc = fd_table[fd];
    if (desc->node->get_type() == node::type::DIRECTORY) {
        return -error::ISDIR;
    }

    return desc->node->get_fs()->read(desc->node, buf, len, desc->pos);
}

vfs::ssize_t vfs::manager::write(ssize_t fd, void *buf, ssize_t len) {
    if (!fd_table.contains(fd)) {
        return -error::INVAL;
    }

    auto desc = fd_table[fd];
    if (desc->node->get_type() == node::type::DIRECTORY) {
        return -error::ISDIR;
    }

    return desc->node->get_fs()->write(desc->node, buf, len, desc->pos);
}

vfs::ssize_t vfs::manager::ioctl(ssize_t fd, size_t req, void *buf) {
    if (!fd_table.contains(fd)){
        return -error::INVAL;
    }

    auto desc = fd_table[fd];
    return desc->node->get_fs()->ioctl(desc->node, req, buf);
}

vfs::node *vfs::manager::get_parent(vfs::path filepath) {
    if (filepath.startswith('/') && filepath.count('/') == 1) {
        return root;
    }
    
    auto parent_path = filepath;
    auto parent_view = frg::string_view{parent_path};
    if (parent_view.find_last('/') != size_t(-1))
        parent_path.resize(frg::string_view{parent_path}.find_last('/'));
        
    auto parent = resolve(parent_path);
    if (!parent) {
        return nullptr;
    }

    return parent;
}

vfs::ssize_t vfs::manager::create(path filepath, int64_t type, int64_t flags) {
    if (!(flags & oflags::DYNAMIC)) {
        if (resolve(filepath)) {
            return open(filepath, flags);
        }
    }

    auto parent = get_parent(filepath);
    if (!parent) {
        return -error::NOENT;
    }

    if (parent->get_type() != node::type::DIRECTORY) {
        return -error::INVAL;
    }

    auto name = split(filepath).pop();
    auto adjusted_filepath = adjust_path(filepath);
    auto fs = parent->get_fs();
    auto node = frg::construct<vfs::node>(memory::mm::heap, fs, name, adjusted_filepath, parent, flags, type);
    fs->nodenames[adjusted_filepath] = node;
    size_t err = 0;
    if (type == node::type::DIRECTORY) {
        if ((err = fs->mkdir(split(filepath), flags)) != 0) {
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

    if (!(flags & oflags::DYNAMIC)) {
        return open(filepath, flags);
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

vfs::ssize_t vfs::manager::open(path filepath, int64_t flags) {
    auto node = follow_links(resolve(filepath));
    if (!node) {
        if (flags & oflags::CREAT) {
            return create(filepath, vfs::node::type::FILE, flags);
        } else {
            return -error::NOENT;
        }
    }

    auto fd = frg::construct<manager::fd>(memory::mm::heap);
    fd->flags = flags;
    fd->pos = 0;
    fd->mode = 0;
    fd->node = node;
    node->ref_count = 1;

    fd_table[fd_table.size() + 1] = fd;
    return fd_table.size();
}

vfs::ssize_t vfs::manager::lstat(path filepath, node::statinfo *buf) {
    auto node = resolve(filepath);
    if (!node) {
        return -error::NOENT;
    }

    memcpy(buf, node->stat(), sizeof(node::statinfo));

    return 0;
}

vfs::ssize_t vfs::manager::close(ssize_t fd) {
    if (!fd_table.contains(fd)) {
        return -error::BADF;
    }
    
    auto desc = fd_table[fd];
    desc->node->ref_count--;
    fd_table.remove(fd);
    frg::destruct(memory::mm::heap, desc);

    return 0;
}

vfs::ssize_t vfs::manager::mkdir(path dirpath, int64_t flags) {
    auto dir = resolve(dirpath);
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

    return create(dirpath, node::type::DIRECTORY, flags);
}

vfs::ssize_t vfs::manager::rename(path oldpath, path newpath, int64_t flags) {
    if (newpath.startswith(oldpath)) {
        return -error::INVAL;
    }

    auto src = resolve(oldpath);
    if (!src) {
        return -error::INVAL;
    }

    auto dst = resolve(newpath);
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

            if ((err = fs->rename(split(oldpath), split(newpath), flags)) != 0) {
                return err;
            }

            src->set_parent(parent);
            src->set_path(path);
            src->set_master(master);

            return 0;
        }
    }

    ssize_t err = 0;
    if ((err = dst->get_fs()->rename(split(oldpath), split(newpath), flags)) != 0) {
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

vfs::ssize_t vfs::manager::link(path from, path to, bool is_symlink) {
    auto src = resolve(from);
    auto dst = resolve(to);

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
        if ((err = fs->link(split(from), split(to), false)) != 0) {
            return err;
        }
    } else {
        if (dst) {
            return -error::EXIST;
        }

        ssize_t err = 0;
        auto fs = resolve_fs(to);
        if ((err = fs->link(split(from), split(to), true)) != 0) {
            return err;
        }

        dst = resolve(to);
        if (src) {
            dst->set_master(src);
        } else {
            dst->set_link(to);
        }
    }

    return 0;
}

vfs::ssize_t vfs::manager::unlink(path filepath) {
    auto dst = resolve(filepath);
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

vfs::ssize_t vfs::manager::in_use(path filepath) {
    auto dst = resolve(filepath);
    if (!dst) {
        return -error::INVAL;
    }

    if (dst->ref_count) {
        return -error::BUSY;
    }

    return 0;
}

vfs::ssize_t vfs::manager::rmdir(path dirpath) {
    auto dir = resolve(dirpath);
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

vfs::pathlist vfs::manager::lsdir(path dirpath) {
    pathlist names{};
    auto dir = resolve(dirpath);
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

vfs::ssize_t vfs::manager::mount(path srcpath, path dstpath, ssize_t fstype, optlist *opts, int64_t flags) {
    auto *src = resolve(srcpath);
    auto *dst = resolve(dstpath);

    switch (flags) {
        case (mflags::NOSRC | mflags::NODST):
        case mflags::NODST:
        case mflags::NOSRC: {
            switch (fstype) {
                case fslist::ROOTFS: {
                    if (root) {
                        return -error::NOTEMPTY;
                    }

                    auto *fs = frg::construct<rootfs>(memory::mm::heap);
                    auto *root = frg::construct<node>(memory::mm::heap, fs, "/", "/", nullptr, 0, node::type::DIRECTORY);
                    fs->init_as_root(root);
                    this->root = root;
                    this->mounts["/"] = fs;
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
                    this->mounts[adjust_path(dstpath)] = fs;
                    break;
                }

                default:
                    return -error::INVAL;
            }
            break;
        }

        case mflags::OVERLAY: {
            size_t ret = umount(dst);
            if (ret) {
                return ret;
            }

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
                    this->mounts[adjust_path(dstpath)] = fs;
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

vfs::ssize_t vfs::manager::umount(node *dst) {
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