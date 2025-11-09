#include "sys/syscall.hpp"
#include "sys/smp.hpp"
#include "util/io.hpp"
#include <cstdint>
#include <util/log/log.hpp>
#include <sys/irq.hpp>

#define LENGTHOF(x) (sizeof(x) / sizeof(x[0]))

extern void syscall_openat(irq::regs *);
extern void syscall_pipe(irq::regs *);
extern void syscall_lseek(irq::regs *);
extern void syscall_dup2(irq::regs *);
extern void syscall_close(irq::regs *);
extern void syscall_read(irq::regs *);
extern void syscall_write(irq::regs *);
extern void syscall_ioctl(irq::regs *);
extern void syscall_lstatat(irq::regs *);
extern void syscall_mkdirat(irq::regs *);
extern void syscall_renameat(irq::regs *);
extern void syscall_linkat(irq::regs *);
extern void syscall_unlinkat(irq::regs *);
extern void syscall_readdir(irq::regs *);
extern void syscall_fcntl(irq::regs *);

extern void syscall_mmap(irq::regs *);
extern void syscall_munmap(irq::regs *);

extern void syscall_exec(irq::regs *);
extern void syscall_fork(irq::regs *);
extern void syscall_exit(irq::regs *);
extern void syscall_futex(irq::regs *r);
extern void syscall_waitpid(irq::regs *);
extern void syscall_usleep(irq::regs *);
extern void syscall_clock_gettime(irq::regs *);
extern void syscall_clock_get(irq::regs *);
extern void syscall_getpid(irq::regs *);
extern void syscall_getppid(irq::regs *);
extern void syscall_gettid(irq::regs *);
extern void syscall_setpgid(irq::regs *);
extern void syscall_getpgid(irq::regs *);
extern void syscall_setsid(irq::regs *);
extern void syscall_getsid(irq::regs *);
extern void syscall_sigenter(irq::regs *);
extern void syscall_sigreturn(irq::regs *);
extern void syscall_sigaction(irq::regs *);
extern void syscall_sigpending(irq::regs *);
extern void syscall_sigprocmask(irq::regs *);
extern void syscall_kill(irq::regs *);
extern void syscall_pause(irq::regs *);
extern void syscall_sigsuspend(irq::regs *);
extern void syscall_getcwd(irq::regs *);
extern void syscall_chdir(irq::regs *);

void syscall_set_fs_base(irq::regs *r) {
    uint64_t addr = r->rdi;
    io::wrmsr(smp::fsBase, addr);
    r->rax = 0;
}

void syscall_get_fs_base(irq::regs *r) {
    r->rax = io::rdmsr<uint64_t>(smp::fsBase);
}

void syscall_set_gs_base(irq::regs *r) {
    uint64_t addr = r->rdi;
    io::wrmsr(smp::gsBase, addr);
    r->rax = 0;
}

void syscall_get_gs_base(irq::regs *r) {
    r->rax = io::rdmsr<uint64_t>(smp::gsBase);
}

void syscall_user_log(irq::regs *r) {
    kmsg("Userspace: ", (char *) r->rdi);
    r->rax = 0;
}

static syscall::handler syscalls_list[] = {
    syscall_openat,
    syscall_close,
    syscall_read,
    syscall_write,
    syscall_lseek,
    syscall_dup2,
    syscall_mmap,
    syscall_munmap,

    syscall_set_fs_base,
    syscall_set_gs_base,
    syscall_get_fs_base,
    syscall_get_gs_base,

    syscall_exit,
    syscall_getpid,
    syscall_gettid,
    syscall_getppid,
    
    syscall_fcntl,
    syscall_lstatat,
    syscall_ioctl,
    syscall_fork,
    syscall_exec,
    syscall_futex,
    syscall_waitpid,
    syscall_readdir,
    syscall_getcwd,
    syscall_chdir,
    nullptr, // TODO: faccesat
    syscall_pipe,
    nullptr, // TODO: umask,
    nullptr, // TODO: uid,
    nullptr, // TODO: euid,
    nullptr, // TODO: suid,
    nullptr, // TODO: seuid,
    nullptr, // TODO: gid,
    nullptr, // TDOO: egid,
    nullptr, // TODO: sgid,
    nullptr, // TODO: segid,

    nullptr, // TODO: chmod,
    nullptr, // TODO: chmodat,

    syscall_sigenter,
    syscall_sigaction,
    syscall_sigpending,
    syscall_sigprocmask,
    syscall_kill,
    syscall_setpgid,
    syscall_getpgid,
    syscall_setsid,
    syscall_getsid,
    syscall_pause,
    syscall_sigsuspend,
    syscall_sigreturn,
    
    syscall_unlinkat,
    syscall_renameat,
    // TODO: symlinkat, readlinkat
    syscall_mkdirat,
    syscall_usleep,
    syscall_clock_gettime,
    syscall_clock_get,
    syscall_linkat,

    syscall_user_log
};

void syscall::register_handler(syscall::handler handler, int syscall) {
    syscalls_list[syscall] = handler;
}

extern "C" {
    void syscall_handler(irq::regs *r) {
        uint64_t syscall_num = r->rax;

        if (syscall_num >= LENGTHOF(syscalls_list)) {
            r->rax = uint64_t(-1);
            smp::set_errno(ENOSYS);
            return;
        }

        // TODO: signal queue
        auto process = smp::get_process();
        process->sig_queue.active = false;

        if (syscalls_list[syscall_num] != nullptr) {
            syscalls_list[syscall_num](r);
        }

        if (r->rax != uint64_t(-1)) {
            smp::set_errno(0);
        }

        process->sig_queue.active = true;
    }
}