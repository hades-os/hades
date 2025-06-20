#include "driver/dtable.hpp"
#include "driver/video/vesa.hpp"
#include "fs/poll.hpp"
#include "ipc/evtable.hpp"
#include "mm/slab.hpp"
#include "util/lock.hpp"
#include "util/types.hpp"
#include <arch/types.hpp>
#include <cstddef>
#include <sys/sched/signal.hpp>
#include <driver/tty/termios.hpp>
#include <driver/tty/tty.hpp>

#include <fs/dev.hpp>
#include <fs/vfs.hpp>
#include <mm/mm.hpp>

tty::device *tty::active_tty = nullptr;

size_t self_major = 5;
size_t self_minor = 0;

void tty::self::init() {
    auto self = prs::construct<tty::self>(prs::allocator{slab::create_resource()}, vfs::devfs::mainbus, dtable::majors::SELF_TTY, -1, nullptr);
    vfs::devfs::append_device(self, dtable::majors::SELF_TTY);
}

ssize_t tty::self::on_open(shared_ptr<vfs::fd> fd, ssize_t flags) {
    if (arch::get_process() && !arch::get_process()->sess->tty) {
        arch::set_errno(ENODEV);
        return -1;
    }

    fd->desc->node = arch::get_process()->sess->tty->file;
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

ssize_t tty::device::on_open(shared_ptr<vfs::fd> fd, ssize_t flags) {
    if (__atomic_fetch_add(&ref, 1, __ATOMIC_RELAXED) == 0) {
        if (driver && driver->has_connect) {
            if (driver->connect(this) == -1) {
                return -1;
            }
        }
    }

    if ((sess == nullptr) && (!((flags & O_NOCTTY) == O_NOCTTY)) &&
        (arch::get_process() && arch::get_process()->group->pgid == arch::get_process()->sess->leader_pgid)) {
        sess = arch::get_process()->sess;
        fg = arch::get_process()->group;

        arch::get_process()->sess->tty = this;
    }

    return 0;
}

ssize_t tty::device::on_close(shared_ptr<vfs::fd> fd, ssize_t flags) {
    if (__atomic_sub_fetch(&ref, 1, __ATOMIC_RELAXED) == 0) {
        if (driver && driver->has_disconnect) {
            driver->disconnect(this);
        }
    }

    return 0;
}

ssize_t tty::device::read(void *buf, size_t count, size_t offset) {
    // TODO: orphans
    if (arch::get_process() && arch::get_process()->sess == sess) {
        if (arch::get_process()->group != fg) {
            if (sched::signal::is_ignored(arch::get_process(), SIGTTIN)
                || sched::signal::is_blocked(arch::get_thread(), SIGTTIN)) {
                arch::set_errno(EIO);
                return -1;
            }

            sched::signal::send_group(nullptr, arch::get_process()->group, SIGTTIN);
            arch::set_errno(EINTR);
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

ssize_t tty::device::write(void *buf, size_t count, size_t offset) {
    // TODO: orphans
    if (arch::get_process() && arch::get_process()->sess == sess) {
        if (arch::get_process()->group != fg && (termios.c_cflag & TOSTOP)) {
            if (sched::signal::is_ignored(arch::get_process(), SIGTTOU)
                || sched::signal::is_blocked(arch::get_thread(), SIGTTOU)) {
                arch::set_errno(EIO);
                return -1;
            }

            sched::signal::send_group(nullptr, arch::get_process()->group, SIGTTOU);
            arch::set_errno(EINTR);
            return -1;
        }
    }

    // TODO: nonblock support in vfs
    out_lock.lock();

    char *chars = (char *) allocator.allocate(count);
    char *chars_ptr = chars;
    auto copied = arch::copy_from_user(chars, buf, count);
    if (copied < count) {
        allocator.deallocate(chars);
        out_lock.unlock();
        return count - copied;
    }

    size_t bytes = 0;
    for (bytes = 0; bytes < count; bytes++) {
        if (!out.push(*chars_ptr++)) {
            out_lock.unlock();
            driver->flush(this);
            out_lock.lock();
        }
    }

    out_lock.unlock();
    driver->flush(this);

    allocator.deallocate(chars);
    return bytes;
}

ssize_t tty::device::ioctl(size_t req, void *buf) {
    util::lock_guard guard{lock};

    switch (req) {
        case TIOCGPGRP: {
            if (arch::get_process()->sess != sess) {
                arch::set_errno(ENOTTY);
                return -1;
            }

            pid_t *pgrp = (pid_t *) buf;
            *pgrp = fg->pgid;
            return 0;
        }

        case TIOCSPGRP: {
            if (arch::get_process()->sess != sess) {
                arch::set_errno(ENOTTY);
                return -1;
            }

            pid_t pgrp = *(pid_t *) buf;
            sched::process_group *group;

            if (!(group = sess->groups[pgrp])) {
                arch::set_errno(EPERM);
                return -1;
            }

            fg = group;
            return 0;
        }

        case TIOCSCTTY: {
            if (sess || (arch::get_process()->sess->leader_pgid != arch::get_process()->group->pgid)) {
                arch::set_errno(EPERM);
                return -1;
            }

            sess = arch::get_process()->sess;
            fg = arch::get_process()->group;

            arch::get_process()->sess->tty = this;            
            return 0;
        }

        case TCGETS: {
            util::lock_guard in_guard{in_lock};
            util::lock_guard out_guard{out_lock};

            tty::termios *attrs = (tty::termios *) buf;
            *attrs = termios;
            return 0;
        }

        case TCSETS: {
            util::lock_guard in_guard{in_lock};
            util::lock_guard out_guard{out_lock};

            tty::termios *attrs = (tty::termios *) buf;
            termios = *attrs;
            return 0;
        }

        case TCSETSW: {
            while (__atomic_load_n(&out.items, __ATOMIC_RELAXED));

            util::lock_guard in_guard{in_lock};
            util::lock_guard out_guard{out_lock};

            tty::termios *attrs = (tty::termios *) buf;
            termios = *attrs;
            return 0;            
        }

        case TCSETSF: {
            while (__atomic_load_n(&out.items, __ATOMIC_RELAXED));

            util::lock_guard in_guard{in_lock};
            util::lock_guard out_guard{out_lock};

            tty::termios *attrs = (tty::termios *) buf;
            termios = *attrs;

            char c;
            while (in.pop(&c));
            return 0;            
        }

        default: {
            if (driver && driver->has_ioctl) {
                auto res = driver->ioctl(this, req, buf);
                return res;
            } else {
                arch::set_errno(ENOTTY);
                return -1;
            }
        }
    }
}

ssize_t tty::device::poll(shared_ptr<poll::producer> producer) {
    outputs.push_back(producer);
    return POLLOUT;
}

void tty::set_active(prs::string_view path, shared_ptr<vfs::fd_table> table) {
    auto fd = vfs::open(vfs::root_ns, nullptr, path, table, O_NOCTTY, O_RDONLY, 0, 0);
    if (!fd) return;

    auto private_data = fd->desc->node->data_as<vfs::devfs::dev_priv>();
    tty::device *tty = (tty::device *) private_data->dev;
    vfs::close(fd);

    if (tty) {
        active_tty = tty;
    }

    if (!video::vesa::disabled) {
        video::vesa::disabled = true;
    }
}