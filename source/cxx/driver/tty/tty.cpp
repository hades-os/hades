#include "fs/dev.hpp"
#include "fs/vfs.hpp"
#include "mm/mm.hpp"
#include "sys/smp.hpp"

#include <cstddef>
#include <sys/sched/signal.hpp>
#include <driver/tty/termios.hpp>
#include <driver/tty/tty.hpp>

tty::device *tty::active_tty = nullptr;
void tty::self::init() {
    auto self = frg::construct<tty::self>(memory::mm::heap);
    vfs::devfs::add("/dev/tty", self);
}

tty::ssize_t tty::self::on_open(vfs::fd *fd, ssize_t flags) {
    if (smp::get_process() && !smp::get_process()->sess->tty) {
        // TODO: errno
        return -1;
    }

    fd->desc->node = smp::get_process()->sess->tty->file;
    return 0;
}

void tty::device::handle_signal(char c) {
    if (termios.c_lflag & ISIG) {
        if (termios.c_cc[VINTR] == c) {
            sched::signal::send_group(nullptr, fg, SIGINT);
        } else if (termios.c_cc[VQUIT] == c) {
            sched::signal::send_group(nullptr, fg, SIGTERM);
        } else if (termios.c_cc[VSUSP] == c) {
            sched::signal::send_group(nullptr, fg, SIGTSTP);
        }
    }
}

void tty::device::set_active() {
    active_tty = this;
}

tty::ssize_t tty::device::on_open(vfs::fd *fd, ssize_t flags) {
    if (__atomic_fetch_add(&ref, 1, __ATOMIC_RELAXED) == 0) {
        if (driver && driver->has_connect) {
            if (driver->connect(this) == -1) {
                return -1;
            }
        }
    }

    if ((sess == nullptr) && (!((flags & vfs::O_NOCTTY) == vfs::O_NOCTTY)) &&
        (smp::get_process() && smp::get_process()->group->pgid == smp::get_process()->sess->leader_pgid)) {
        sess = smp::get_process()->sess;
        fg = smp::get_process()->group;
    }

    return 0;
}

tty::ssize_t tty::device::on_close(vfs::fd *fd, ssize_t flags) {
    if (__atomic_sub_fetch(&ref, 1, __ATOMIC_RELAXED) == 0) {
        if (driver && driver->has_disconnect) {
            driver->disconnect(this);
        }
    }

    return 0;
}

tty::ssize_t tty::device::read(void *buf, size_t count, size_t offset) {
    // TODO: orphans
    if (smp::get_process() && smp::get_process()->sess == sess) {
        if (smp::get_process()->group != fg) {
            if (sched::signal::is_ignored(smp::get_process(), SIGTTIN)
                || sched::signal::is_blocked(smp::get_process(), SIGTTIN)) {
                // TODO: errno
                return -1;
            }

            sched::signal::send_group(nullptr, smp::get_process()->group, SIGTTIN);
            // TODO: errno
            return -1;
        }
    }

    if (termios.c_lflag & ICANON) {
        return read_canon(buf, count);
    } else {
        return read_raw(buf, count);
    }

    return 0;
}

tty::ssize_t tty::device::write(void *buf, size_t count, size_t offset) {
    // TODO: orphans
    if (smp::get_process() && smp::get_process()->sess == sess) {
        if (smp::get_process()->group != fg && (termios.c_cflag & TOSTOP)) {
            if (sched::signal::is_ignored(smp::get_process(), SIGTTOU)
                || sched::signal::is_blocked(smp::get_process(), SIGTTOU)) {
                // TODO: errno
                return -1;
            }

            sched::signal::send_group(nullptr, smp::get_process()->group, SIGTTOU);
            // TODO: errno
            return -1;
        }
    }

    // TODO: nonblock support in vfs
    out_lock.irq_acquire();

    char *chars = (char *) buf;
    size_t bytes = 0;
    for (bytes = 0; bytes < count; bytes++) {
        if (!out.push(*chars++)) {
            out_lock.irq_release();
            driver->flush(this);
            out_lock.irq_acquire();
        }
    }

    out_lock.irq_release();
    driver->flush(this);

    return bytes;
}

tty::ssize_t tty::device::ioctl(size_t req, void *buf) {
    lock.irq_acquire();
    switch (req) {
        case TIOCGPGRP: {
            if (smp::get_process()->sess != sess) {
                // TODO: errno
                lock.irq_release();
                return -1;
            }

            sched::pid_t *pgrp = (sched::pid_t *) buf;
            *pgrp = fg->pgid;
            lock.irq_release();
            return 0;
        }

        case TIOCSPGRP: {
            if (smp::get_process()->sess != sess) {
                // TODO: errno
                lock.irq_release();
                return -1;
            }

            sched::pid_t pgrp = *(sched::pid_t *) buf;
            sched::process_group *group;

            if (!(group = sess->groups[pgrp])) {
                // TODO: errno
                lock.irq_release();
                return -1;
            }

            fg = group;
            lock.irq_release();
            return 0;
        }

        case TIOCSCTTY: {
            if (sess || (smp::get_process()->sess->leader_pgid != smp::get_process()->group->pgid)) {
                // TODO: errno
                lock.irq_release();
                return -1;
            }

            sess = smp::get_process()->sess;
            fg = smp::get_process()->group;
            lock.irq_release();
            return 0;
        }

        case TCGETS: {
            in_lock.irq_acquire();
            out_lock.irq_acquire();

            tty::termios *attrs = (tty::termios *) buf;
            *attrs = termios;

            out_lock.irq_release();
            in_lock.irq_release();

            lock.irq_release();
            return 0;
        }

        case TCSETSW: {
            while (__atomic_load_n(&out.items, __ATOMIC_RELAXED));

            in_lock.irq_acquire();
            out_lock.irq_acquire();

            tty::termios *attrs = (tty::termios *) buf;
            termios = *attrs;

            out_lock.irq_release();
            in_lock.irq_release();

            lock.irq_release();
            return 0;            
        }

        case TCSETSF: {
            while (__atomic_load_n(&out.items, __ATOMIC_RELAXED));

            in_lock.irq_acquire();
            out_lock.irq_acquire();

            tty::termios *attrs = (tty::termios *) buf;
            termios = *attrs;

            char c;
            while (in.pop(&c));
            out_lock.irq_release();
            in_lock.irq_release();

            lock.irq_release();
            return 0;            
        }
        
        case SET_ACTIVE: {
            set_active();
            lock.irq_release();
            return 0;
        }

        default: {
            if (driver && driver->has_ioctl) {
                auto res = driver->ioctl(this, req, buf);
                lock.irq_release();
                return res;
            } else {
                // TODO: errno
                lock.irq_release();
                return -1;
            }
        }
    }
}