/*
    vfs::fd *open(frg::string_view filepath, fd_table *table, int64_t flags, int64_t mode);
    fd_pair open_pipe(fd_table *table, ssize_t flags);
    ssize_t lseek(vfs::fd *fd, size_t off, size_t whence);
    vfs::fd *dup(vfs::fd *fd, ssize_t flags, ssize_t new_num);
    ssize_t close(vfs::fd *fd);
    ssize_t read(vfs::fd *fd, void *buf, ssize_t len);
    ssize_t write(vfs::fd *fd, void *buf, ssize_t len);
    ssize_t ioctl(vfs::fd *fd, size_t req, void *buf);
    void *mmap(vfs::fd *fd, void *addr, ssize_t off, ssize_t len);
    ssize_t lstat(frg::string_view filepath, node::statinfo *buf);
    ssize_t create(frg::string_view filepath, fd_table *table, int64_t type, int64_t flags, int64_t mode);
    ssize_t mkdir(frg::string_view dirpath, int64_t flags, int64_t mode);
    ssize_t rename(frg::string_view oldpath, frg::string_view newpath, int64_t flags);
    ssize_t link(frg::string_view from, frg::string_view to, bool is_symlink);
    ssize_t unlink(frg::string_view filepath);
    ssize_t rmdir(frg::string_view dirpath);
    pathlist lsdir(frg::string_view dirpath);
 */

#include <fs/vfs.hpp>
#include <sys/irq.hpp>

void syscall_open(irq::regs *r) {
    
}