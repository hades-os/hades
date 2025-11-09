#ifndef SCHED_HPP
#define SCHED_HPP

#include "sys/sched/wait.hpp"
#include <cstddef>
#include <cstdint>
#include <frg/hash.hpp>
#include <frg/hash_map.hpp>
#include <frg/rbtree.hpp>
#include <frg/vector.hpp>
#include <fs/vfs.hpp>
#include <mm/mm.hpp>
#include <mm/vmm.hpp>
#include <sys/irq.hpp>
#include <sys/sched/wait.hpp>
#include <sys/sched/signal.hpp>
#include <sys/sched/regs.hpp>
#include <util/lock.hpp>
#include <util/elf.hpp>
#include <util/types.hpp>

namespace tty {
    struct device;
}

namespace sched {
    inline volatile size_t uptime;

    constexpr size_t PIT_FREQ = 1000;

    constexpr size_t WNOHANG = 1;
    constexpr size_t WUNTRACED = 2;
    constexpr size_t WSTOPPED = 2;
    constexpr size_t WEXITED = 4;
    constexpr size_t WCONTINUED = 8;
    constexpr size_t WNOWAIT = 0x01000000;

    constexpr size_t WCOREFLAG = 0x80;

    constexpr ssize_t WEXITSTATUS(ssize_t x) { return (x & 0xff00) >> 8;  }
    constexpr ssize_t WTERMSIG(ssize_t x) { return x & 0x7F; }
    constexpr ssize_t WSTOPSIG(ssize_t x) { return WEXITSTATUS(x); }
    constexpr ssize_t WIFEXITED(ssize_t x) { return WTERMSIG(x) == 0; }
    constexpr ssize_t WIFSIGNALED(ssize_t x) { return ((signed char)(((x) & 0x7f) + 1) >> 1) > 0; }
    constexpr ssize_t WIFSTOPPED(ssize_t x) { return ((x) & 0xff) == 0x7f; }
    constexpr ssize_t WIFCONTINUED(ssize_t x) { return x == 0xffff; }
    constexpr ssize_t WCOREDUMP(ssize_t x) { return x & WCOREFLAG; }

    constexpr ssize_t WSTATUS_CONSTRUCT(ssize_t x) { return x << 8; }
    constexpr ssize_t WEXITED_CONSTRUCT(ssize_t x) { return WSTATUS_CONSTRUCT(x); } 
    constexpr ssize_t WSIGNALED_CONSTRUCT(ssize_t x) { return x & 0x7F; } 
    constexpr ssize_t WSTOPPED_CONSTRUCT = 0x7F;
    constexpr ssize_t WCONTINUED_CONSTRUCT = 0xffff;

    constexpr size_t STATUS_CHANGED = (1ULL << 31);

    constexpr size_t FUTEX_WAIT = 0;
    constexpr size_t FUTEX_WAKE = 1;

    class session;
    class thread;
    class process;
    class process_group;

    void sleep(size_t time);
    void init();

    void send_ipis();

    void init_syscalls();
    void init_idle();
    void init_sse();

    void init_bsp();
    void init_ap();

    thread *create_thread(void (*main)(), uint64_t rsp, memory::vmm::vmm_ctx *ctx, uint8_t privilege);
    process *create_process(char *name, void (*main)(), uint64_t rsp, memory::vmm::vmm_ctx *ctx, uint8_t privilege);

    thread *fork(thread *original, memory::vmm::vmm_ctx *ctx);
    process *fork(process *original, thread *caller);

    int do_futex(uintptr_t vaddr, int op, int expected, timespec *timeout);    

    process *find_process(pid_t pid);

    int64_t pick_task();
    void swap_task(irq::regs *r);
    void tick_bsp(irq::regs *r);
    void retick();

    uint16_t get_fcw();
    void set_fcw(uint16_t);

    uint32_t get_mxcsr();
    void set_mxcsr(uint32_t mxcsr);

    void save_sse(char *sse_region);
    void load_sse(char *sse_region);

    struct futex {
        util::lock lock;
        ipc::queue waitq;
        ipc::trigger trigger;
        uint64_t paddr;

        int locked;
    };

    struct [[gnu::packed]] thread_info {
        uint64_t meta_ptr;

        int errno;
        tid_t tid;

        size_t started;
        size_t stopped;
        size_t uptime;
    };

    class thread_env {
        public:
            char **argv;
            char **env;

            int argc;
            int envc;
    };

    class thread {
        public:
            regs reg;

            alignas(16)
            char sse_region[512];

            signal::ucontext sig_context;

            uintptr_t sig_kstack;
            uintptr_t sig_ustack;

            uintptr_t kstack;
            uintptr_t ustack;

            memory::vmm::vmm_ctx *mem_ctx;

            uint64_t started;
            uint64_t stopped;
            uint64_t uptime;

            enum state {
                READY,
                RUNNING,
                SLEEP,
                BLOCKED,
                DEAD,
                WAIT
            };

            uint8_t state;
            int64_t cpu;

            tid_t tid;
            pid_t pid;

            process *proc;
            
            uint8_t privilege;
            bool running;
            thread_env env;

            int64_t start();
            
            void stop();
            void cont();

            int64_t kill();
    };

    struct process_env {
        elf::file file;
        elf::file interp;

        char *file_path;
        char *interp_path;
        bool has_interp;

        struct {
            int envc;
            int argc;

            char **argv;
            char **envp;
        } params;

        uint64_t entry;
        bool is_loaded;

        process *proc;

        bool load_elf(const char *path, vfs::fd *fd);
        void place_params(char **envp, char **argv, thread *target);

        uint64_t *place_args(uint64_t* location);
        uint64_t *place_auxv(uint64_t *location);
        void load_params(char **argv, char** envp);
    };

    class process {
        public:
            char name[50];

            memory::vmm::vmm_ctx *mem_ctx;

            frg::vector<thread *, memory::mm::heap_allocator> threads;
            frg::vector<process *, memory::mm::heap_allocator> children;
            frg::vector<process *, memory::mm::heap_allocator> zombies;            
            vfs::fd_table *fds;
            vfs::node *cwd;

            util::lock lock{};

            uint64_t started;
            uint64_t stopped;

            bool did_exec;

            thread *main_thread;
            process *parent;
            process_group *group;
            session *sess;

            bool block_signals;
            uint64_t sigenter_rip;
            util::lock sig_lock;
            signal::queue sig_queue;
            signal::sigaction sigactions[SIGNAL_MAX];

            ipc::queue *waitq;
            ipc::trigger *notify_status;

            pid_t pid;
            pid_t ppid;
            pid_t pgid;
            pid_t sid;

            uid_t real_uid;
            uid_t effective_uid;
            gid_t saved_gid;

            uint8_t privilege;

            ssize_t status;
            process_env env; 

            int64_t start();
            void kill(int exit_code = 0);

            void suspend();
            void cont();

            void spawn(void (*main)(), uint64_t rsp, uint8_t privilege);
            void add_thread(thread *task);
            void kill_thread(int64_t tid);
            thread *pick_thread();
            size_t find_child(process *proc);
            size_t find_zombie(process *proc);

            frg::tuple<int, pid_t> waitpid(pid_t pid, thread *waiter, int options);
    };

    class process_group {
        public:
            pid_t pgid;

            pid_t leader_pid;
            process *leader;

            session *sess;
            frg::vector<process *, memory::mm::heap_allocator> procs;
    };

    class session {
        public:
            pid_t sid;
            pid_t leader_pgid;
            frg::vector<process_group *, memory::mm::heap_allocator> groups;

            tty::device *tty;
    };

    inline frg::vector<sched::process *, memory::mm::heap_allocator> processes{};
    inline frg::vector<sched::thread *, memory::mm::heap_allocator> threads{};

    extern util::lock sched_lock;

    constexpr uint64_t EFER = 0xC0000080;
    constexpr uint64_t STAR = 0xC0000081;
    constexpr uint64_t LSTAR = 0xC0000082;
    constexpr uint64_t SFMASK = 0xC0000084;
};

#endif