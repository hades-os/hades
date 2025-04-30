#ifndef TYPES_HPP
#define TYPES_HPP

#include <cstddef>
#include <cstdint>
#include <frg/unique.hpp>
#include <mm/mm.hpp>
#include <prs/shared.hpp>

using pid_t = int;
using tid_t = int;
using uid_t = uint32_t;
using gid_t = int32_t;

using blksize_t = int64_t;
using blkcnt_t = int64_t;

using dev_t = uint64_t;
using ino_t = uint64_t;
using mode_t = int32_t;
using nlink_t = int32_t;
using clockid_t = uint64_t;
using time_t = long;

using sigset_t = uint64_t;
using ssize_t = int64_t;
using off_t = int64_t;

constexpr int S_IRWXU = 0700;
constexpr int S_IRUSR = 0400;
constexpr int S_IWUSR = 0200;
constexpr int S_IXUSR = 0100;
constexpr int S_IRWXG = 070;
constexpr int S_IRGRP = 040;
constexpr int S_IWGRP = 020;
constexpr int S_IXGRP = 010;
constexpr int S_IRWXO = 07;
constexpr int S_IROTH = 04;
constexpr int S_IWOTH = 02;
constexpr int S_IXOTH = 01;

constexpr int S_ISUID = 04000;
constexpr int S_ISGID = 02000;
constexpr int S_ISVTX = 01000;

constexpr int S_IFMT     = 0x0f000;
constexpr int S_IFBLK   = 0x06000;
constexpr int S_IFCHR   = 0x02000;
constexpr int S_IFIFO   = 0x01000;
constexpr int S_IFREG   = 0x08000;
constexpr int S_IFDIR   = 0x04000;
constexpr int S_IFLNK   = 0x0a000;
constexpr int S_IFSOCK = 0x0c000;

constexpr int DEFAULT_MODE = S_IRWXU | S_IRWXG | S_IRWXO;

constexpr int O_PATH = 010000000;
constexpr int O_ACCMODE = (03 | O_PATH);

constexpr int O_RDONLY = 00;
constexpr int O_WRONLY = 01;
constexpr int O_RDWR = 02;

constexpr int O_CREAT = 0100;
constexpr int O_EXCL = 0200;
constexpr int O_NOCTTY = 0400;

constexpr int O_TRUNC = 01000;
constexpr int O_APPEND = 02000;
constexpr int O_NONBLOCK = 04000;
constexpr int O_DSYNC = 010000;
constexpr int O_ASYNC = 020000;
constexpr int O_DIRECT = 040000;
constexpr int O_DIRECTORY = 0200000;
constexpr int O_NOFOLLOW = 0400000;
constexpr int O_CLOEXEC = 02000000;

constexpr int O_SYNC = 04010000;
constexpr int O_RSYNC = 04010000;
constexpr int O_LARGEFILE = 0100000;
constexpr int O_NOATIME = 01000000;
constexpr int O_TMPFILE = 020000000;

constexpr int O_EXEC = O_PATH;
constexpr int O_SEARCH = O_PATH;

constexpr int SEEK_SET = 1;
constexpr int SEEK_CUR = 2;
constexpr int SEEK_END = 3;

constexpr int F_OK = 1;
constexpr int R_OK = 2;
constexpr int W_OK = 4;
constexpr int X_OK = 8;

constexpr int F_DUPFD = 0;
constexpr int F_GETFD = 1;
constexpr int F_SETFD = 2;
constexpr int F_GETFL = 3;
constexpr int F_SETFL = 4;

constexpr int F_SETOWN = 8;
constexpr int F_GETOWN = 9;
constexpr int F_SETSIG = 10;
constexpr int F_GETSIG = 11;

constexpr int F_GETLK = 5;
constexpr int F_SETLK = 6;
constexpr int F_SETLKW = 7;

constexpr int F_SETOWN_EX = 15;
constexpr int F_GETOWN_EX = 16;

constexpr int F_GETOWNER_UIDS = 17;

constexpr int F_SETLEASE = 1024;
constexpr int F_GETLEASE = 1025;
constexpr int F_NOTIFY = 1026;
constexpr int F_DUPFD_CLOEXEC = 1030;
constexpr int F_SETPIPE_SZ = 1031;
constexpr int F_GETPIPE_SZ = 1032;
constexpr int F_ADD_SEALS = 1033;
constexpr int F_GET_SEALS = 1034;

constexpr int F_SEAL_SEAL = 0x0001;
constexpr int F_SEAL_SHRINK = 0x0002;
constexpr int F_SEAL_GROW = 0x0004;
constexpr int F_SEAL_WRITE = 0x0008;

constexpr int F_OFD_GETLK = 36;
constexpr int F_OFD_SETLK = 37;
constexpr int F_OFD_SETLKW = 38;

constexpr int F_RDLCK = 0;
constexpr int F_WRLCK = 1;
constexpr int F_UNLCK = 2;
constexpr int FD_CLOEXEC = 1;

constexpr int AT_FDCWD = -100;
constexpr int AT_EMPTY_PATH = 0x1000;
constexpr int AT_SYMLINK_FOLLOW = 0x400;
constexpr int AT_SYMLINK_NOFOLLOW = 0x100;
constexpr int AT_REMOVEDIR = 0x200;
constexpr int AT_EACCESS = 0x200;
constexpr int AT_NO_AUTOMOUNT = 0x800;
constexpr int AT_STATX_SYNC_AS_STAT = 0;
constexpr int AT_STATX_FORCE_SYNC = 0x2000;
constexpr int AT_STATX_DONT_SYNC = 0x4000;
constexpr int AT_STATX_SYNC_TYPE = 0x6000;

constexpr size_t DT_UNKNOWN = 0;
constexpr size_t DT_FIFO = 1;
constexpr size_t DT_CHR = 2;
constexpr size_t DT_DIR = 4;
constexpr size_t DT_BLK = 6;
constexpr size_t DT_REG = 8;
constexpr size_t DT_LNK = 10;
constexpr size_t DT_SOCK = 12;
constexpr size_t DT_WHT = 14;

struct dirent {
    ino_t d_ino;
    off_t d_off;
    uint16_t d_reclen;
    uint8_t d_type;
    char d_name[1024];
};

constexpr size_t POLLIN = 0x01;
constexpr size_t POLLOUT = 0x02;
constexpr size_t POLLPRI = 0x04;
constexpr size_t POLLHUP = 0x08;
constexpr size_t POLLERR = 0x10;
constexpr size_t POLLRDHUP = 0x20;
constexpr size_t POLLNVAL = 0x40;
constexpr size_t POLLWRNORM = 0x80;

using nfds_t = size_t;
struct pollfd {
    int fd;
    short events;
    short revents;
};

using bus_addr_t = size_t;
using bus_size_t = size_t;
using bus_handle_t = uintptr_t;

template<typename T, typename A>
using unique_ptr = prs::unique_ptr<T, A>;

template <typename T>
using shared_ptr = prs::shared_ptr<T>;

template<typename  T>
using weak_ptr = prs::weak_ptr<T>;

constexpr char alpha_lower[] = "abcdefghijklmnopqrstuvwxyz";

struct function_t {
    template<class...Ts>
    constexpr void operator()(Ts&&...) const {}
    
    explicit constexpr operator bool() const { return false; }
    constexpr function_t() {}

    friend constexpr bool operator==(::std::nullptr_t, function_t ) { return true; }
    friend constexpr bool operator==(function_t, ::std::nullptr_t ) { return true; }
    friend constexpr bool operator!=(::std::nullptr_t, function_t ) { return false; }
    friend constexpr bool operator!=(function_t, ::std::nullptr_t ) { return false; }
};

constexpr function_t function{};

#endif