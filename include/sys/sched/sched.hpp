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

namespace tty {
    struct device;
}

namespace sched {
    using pid_t = int64_t;
    using tid_t = int64_t;
    using uid_t = int32_t;
    using gid_t = int32_t;

    inline volatile size_t uptime;

    constexpr size_t PIT_FREQ = 1000;

    class session;
    class thread;
    class process;
    class process_group;

    void sleep(size_t time);
    void init();

    void send_ipis();
    void init_idle();

    void init_syscalls();
    void init_locals();

    void init_bsp();
    void init_ap();

    thread *create_thread(void (*main)(), uint64_t rsp, memory::vmm::vmm_ctx *ctx, uint8_t privilege);
    process *create_process(char *name, void (*main)(), uint64_t rsp, memory::vmm::vmm_ctx *ctx, uint8_t privilege);

    bool exec(thread *target, const char *path, char **argv, char **envp);
    thread *fork(thread *original, memory::vmm::vmm_ctx *ctx);
    process *fork(process *original, thread *caller);

    process *find_process(pid_t pid);

    int64_t pick_task();
    void swap_task(irq::regs *r);
    void tick_bsp(irq::regs *r);
    void retick();

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

    enum wait_opts {
        WNOHANG = 1,
        WUNTRACED,
        WSTOPPED,
        WIFEXITED,
        WCONTINUED,
        WNOWAIT
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
            
            uint64_t user_fs;
            uint64_t user_gs;

            bool did_exec;

            thread *main_thread;
            process *parent;
            process_group *group;
            session *sess;

            bool block_signals;
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

            enum state_opts {
                STOPPED,
                RUNNING,
                TERMINATED,

                STATUS_CHANGED = (1 << 31)
            };

            enum messages {
                SUSPEND,
                STARTED,
                KILLED
            };

            struct state {
                uint8_t term_signal;
                uint8_t stop_signal;
                
                uint32_t val;
            };

            state status;
            process_env env; 

            int64_t start();
            void kill();

            void suspend();
            void cont();

            void spawn(void (*main)(), uint64_t rsp, uint8_t privilege);
            void add_thread(thread *task);
            void kill_thread(int64_t tid);
            thread *pick_thread();
            size_t find_child(process *proc);
            size_t find_zombie(process *proc);

            frg::tuple<uint32_t, pid_t> waitpid(pid_t pid, thread *waiter, int options);
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