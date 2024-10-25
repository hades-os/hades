#ifndef PTY_HPP
#define PTY_HPP

#include <fs/dev.hpp>
#include <util/ring.hpp>
#include <cstddef>
#include <driver/tty/tty.hpp>
#include <util/lock.hpp>

namespace tty {
    constexpr size_t ptmx_major = 5;
    constexpr size_t ptmx_minor = 2;

    constexpr size_t pts_major = 136;

    struct ptm;
    struct pts: driver {
        private:
            size_t slave_no;
            device *tty;
            ptm *master;
            winsize size;
        public:
            friend struct ptmx;
            friend struct ptm;

            ssize_t ioctl(device *tty, size_t req, void *buf) override; 
            void flush(tty::device *tty) override;
    };

    struct ptm: vfs::devfs::device {
        private:
            util::lock in_lock;
            util::ring<char> in;
            pts *slave;
        public:
            friend struct ptmx;
            friend struct pts;

            ptm(): in_lock(), in(max_chars), slave(nullptr) {};
            ssize_t read(void *buf, ssize_t len, ssize_t offset) override;
            ssize_t write(void *buf, ssize_t len, ssize_t offset) override; 
            ssize_t ioctl(size_t req, void *buf) override;        
    };

    static util::lock ptmx_lock{};
    static size_t last_pts = 0;
    static size_t last_ptm = 0;
    struct ptmx: vfs::devfs::device {
        public:
            static void init();
            ssize_t on_open(vfs::fd *fd, ssize_t flags) override;
    };
};

#endif