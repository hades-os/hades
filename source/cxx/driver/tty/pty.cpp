#include "driver/tty/tty.hpp"
#include "fs/dev.hpp"
#include "fs/vfs.hpp"
#include "mm/mm.hpp"
#include "sys/smp.hpp"
#include <cstddef>
#include <driver/tty/pty.hpp>

void tty::ptmx::init() {
    ptmx *device = frg::construct<ptmx>(memory::mm::heap);
    device->blockdev = false;
    device->major = ptmx_major;
    device->minor = ptmx_minor;
    device->name = "ptmx";

    vfs::devfs::add(device);    
}

tty::ssize_t tty::ptmx::on_open(vfs::fd *fd, tty::ssize_t flags) {
    ptmx_lock.irq_acquire();
    size_t slave_no = last_pts++;

    tty::device *pts_tty = frg::construct<tty::device>(memory::mm::heap);
    tty::pts *pts = frg::construct<tty::pts>(memory::mm::heap);
    tty::ptm *ptm = frg::construct<tty::ptm>(memory::mm::heap);

    ptm->resolveable = false;
    ptm->name = vfs::path("ptm") + ((last_ptm++) + 48);
    vfs::devfs::add(ptm);
    vfs::node *ptm_node = frg::construct<vfs::node>(memory::mm::heap, fd->desc->node->get_fs(), ptm->name, "", fd->desc->node, 0, vfs::node::type::CHARDEV);
    fd->desc->node = ptm_node;

    pts_tty->driver = pts;

    pts->slave_no = slave_no;
    pts->tty = pts_tty;
    pts->master = ptm;
    pts->has_flush = true;
    pts->has_ioctl = true;

    vfs::path pts_path("/dev/pts/");
    pts_path += slave_no;
    pts_tty->name = vfs::path("") + (slave_no + 48);
    vfs::devfs::add(pts_tty);
    vfs::insert_node(pts_path, vfs::node::type::CHARDEV);

    ptmx_lock.irq_release();

    return 0;
}

void tty::pts::flush(tty::device *tty) {
    auto ptm = master;
    char c;

    tty->out_lock.irq_acquire();
    ptm->in_lock.irq_acquire();

    while (tty->out.pop(&c)) {
        if (!ptm->in.push(c)) {
            break;
        }
    }

    ptm->in_lock.irq_release();
    tty->out_lock.irq_release();
}

tty::ssize_t tty::ptm::read(void *buf, ssize_t len, ssize_t offset) {
    ssize_t count;
    char *chars = (char *) buf;

    in_lock.irq_acquire();
    for (count = 0; count < len; count++) {
        if (!in.pop(chars)) {
            break;
        }

        chars++;
    }
    
    in_lock.irq_release();
    return count;
}

tty::ssize_t tty::ptm::write(void *buf, ssize_t len, ssize_t offset) {
    ssize_t count;
    char *chars = (char *) buf;

    in_lock.irq_acquire();
    for (count = 0; count < len; count++) {
        if (!in.push(*chars)) {
            break;
        }

        chars++;
    }
    
    in_lock.irq_release();
    return count;
}

tty::ssize_t tty::ptm::ioctl(size_t req, void *buf) {
    auto pts = slave;

    switch (req) {
        case TIOCGPTN: {
            size_t *ptn = (size_t *) buf;
            *ptn = pts->slave_no;

            return 0;
        }

        case TIOCGWINSZ: {
            memcpy(buf, &pts->size, sizeof(winsize));
            return 0;
        }

        case TIOCSWINSZ: {
            memcpy(&pts->size, buf, sizeof(winsize));
            return 0;
        }

        default: {
            // TODO: errno
            return -1;
        }
    }
}

tty::ssize_t tty::pts::ioctl(device *tty, size_t req, void *buf) {
    switch (req) {
        case TIOCGWINSZ: {
            memcpy(buf, &size, sizeof(winsize));
            return 0;
        }

        case TIOCSWINSZ: {
            memcpy(&size, buf, sizeof(winsize));
            return 0;
        }

        default: {
            // TODO: errno
            return -1;
        }
    };
}