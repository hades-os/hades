#include "driver/dtable.hpp"
#include "mm/slab.hpp"
#include "util/lock.hpp"
#include "util/types.hpp"
#include <driver/tty/tty.hpp>
#include <fs/dev.hpp>
#include <fs/vfs.hpp>
#include <mm/mm.hpp>
#include <cstddef>
#include <driver/tty/pty.hpp>

util::spinlock ptmx_lock{};
void tty::ptmx::init() {
    ptmx *device = prs::construct<ptmx>(prs::allocator{slab::create_resource()}, vfs::devfs::mainbus, dtable::majors::PTMX, -1, nullptr);
    vfs::devfs::append_device(device, dtable::majors::PTMX);
}

ssize_t tty::ptmx::on_open(shared_ptr<vfs::fd> fd, ssize_t flags) {
    util::lock_guard ptmx_guard{ptmx_lock};

    tty::pts *pts = prs::construct<tty::pts>(prs::allocator{slab::create_resource()});
    tty::ptm *ptm = prs::construct<tty::ptm>(prs::allocator{slab::create_resource()}, vfs::devfs::mainbus, dtable::majors::PTM, -1, pts);
    tty::device *pts_tty = prs::construct<tty::device>(prs::allocator{slab::create_resource()}, vfs::devfs::mainbus, dtable::majors::PTS, -1, pts);

    pts->tty = pts_tty;
    pts->master = ptm;
    pts->has_flush = true;
    pts->has_ioctl = true;

    vfs::devfs::append_device(ptm, dtable::majors::PTM);
    vfs::devfs::append_device(pts_tty, dtable::majors::PTS);

    fd->desc->node = ptm->file;
    return 0;
}

void tty::pts::flush(tty::device *tty) {
    auto ptm = master;
    char c;

    util::lock_guard out_guard{tty->out_lock};
    util::lock_guard in_guard{ptm->in_lock};

    while (tty->out.pop(&c)) {
        if (!ptm->in.push(c)) {
            break;
        }
    }
}

ssize_t tty::ptm::read(void *buf, size_t len, size_t offset) {
    size_t count;
    char *chars = (char *) allocator.allocate(len);
    char *chars_ptr = chars;

    in_lock.lock();
    for (count = 0; count < len; count++) {
        if (!in.pop(chars_ptr)) {
            break;
        }

        chars_ptr++;
    }

    in_lock.unlock();

    auto copied = arch::copy_to_user(buf, chars, count);
    if (copied < count) {
        allocator.deallocate(chars);
        return count - copied;
    }

    allocator.deallocate(chars);
    return count;
}

ssize_t tty::ptm::write(void *buf, size_t len, size_t offset) {
    size_t count;
    char *chars = (char *) allocator.allocate(len);
    char *chars_ptr = chars;

    auto copied = arch::copy_from_user(chars, buf, len);
    if (copied < len) {
        allocator.deallocate(chars);
        return len - copied;
    }

    util::lock_guard slave_guard{slave->tty->in_lock};

    for (count = 0; count < len; count++) {
        if (!slave->tty->in.push(*chars_ptr)) {
            break;
        }

        chars_ptr++;
    }

    allocator.deallocate(chars);
    return count;
}

ssize_t tty::ptm::ioctl(size_t req, void *buf) {
    auto pts = slave;

    switch (req) {
        case TIOCGPTN: {
            size_t *ptn = (size_t *) buf;
            *ptn = minor;

            return 0;
        }

        case TIOCGWINSZ: {
            auto copied = arch::copy_to_user(buf, &pts->size, sizeof(winsize));
            if (copied < sizeof(winsize)) {
                return -1;
            }

            return 0;
        }

        case TIOCSWINSZ: {
            auto copied = arch::copy_from_user(&pts->size, buf, sizeof(winsize));
            if (copied < sizeof(winsize)) {
                return -1;
            }

            return 0;
        }

        default: {
            arch::set_errno(ENOSYS);
            return -1;
        }
    }
}

ssize_t tty::pts::ioctl(device *tty, size_t req, void *buf) {
    switch (req) {
        case TIOCGWINSZ: {
            auto copied = arch::copy_to_user(buf, &size, sizeof(winsize));
            if (copied < sizeof(winsize)) {
                return -1;
            }

            return 0;
        }

        case TIOCSWINSZ: {
            auto copied = arch::copy_from_user(&size, buf, sizeof(winsize));
            if (copied < sizeof(winsize)) {
                return -1;
            }
            
            return 0;
        }

        default: {
            arch::set_errno(ENOSYS);
            return -1;
        }
    };
}