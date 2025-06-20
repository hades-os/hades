#ifndef SCHED_HPP
#define SCHED_HPP

#include "arch/types.hpp"
#include "mm/arena.hpp"
#include "prs/allocator.hpp"
#include "prs/rbtree.hpp"
#include <arch/x86/types.hpp>
#include <ipc/wire.hpp>
#include <cstddef>
#include <cstdint>
#include <frg/hash.hpp>
#include <frg/hash_map.hpp>
#include <fs/vfs.hpp>
#include <mm/mm.hpp>
#include <mm/vmm.hpp>
#include <sys/sched/signal.hpp>
#include <util/lock.hpp>
#include <util/elf.hpp>
#include <util/types.hpp>

namespace tty {
    struct device;
}

namespace ns {
    struct accessor;
}

namespace sched {
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

    struct session;
    struct thread;
    struct process;
    struct process_group;

    void init();

    thread *create_thread(void (*main)(), uint64_t rsp, vmm::vmm_ctx *ctx, uint8_t privilege, bool assign_tid = true);

    thread *fork(thread *original, vmm::vmm_ctx *ctx, arch::irq_regs *r);
    process *fork(process *original, thread *caller, arch::irq_regs *r);

    int do_futex(uintptr_t vaddr, int op, uint32_t expected, timespec *timeout);    

    frg::tuple<tid_t, thread *> pick_task();
    void swap_task(arch::irq_regs *r);
    void balance_tasks();

    struct futex {
        util::spinlock lock;
        uint64_t paddr;
        ipc::wire wire;   

        int locked;

        futex(uint64_t paddr): lock(), paddr(paddr), wire(), locked(0) {};
    };

    struct [[gnu::packed]] thread_info {
        uint64_t meta_ptr;

        int errno;
        tid_t tid;

        size_t started;
        size_t stopped;
        size_t uptime;
    };

    struct thread {
        public:
            arch::thread_ctx ctx;

            signal::thread_ctx sig_ctx;
            signal::ucontext ucontext;
            uintptr_t sig_kstack;
            uintptr_t sig_ustack;

            uintptr_t kstack;
            uintptr_t ustack;

            vmm::vmm_ctx *mem_ctx;

            uint64_t started;
            uint64_t stopped;
            uint64_t uptime;

            enum state {
                READY,
                RUNNING,
                SLEEP,
                BLOCKED,
                DEAD,
            };

            bool pending_signal;
            bool dispatch_ready;
            bool in_syscall;

            uint8_t state;
            bool running;

            process *proc;
            tid_t tid;
            pid_t pid;

            ipc::wire wire;
            prs::rbtree_hook hook;

            void start();
            void stop();
            void cont();

            thread(uintptr_t kstack, uintptr_t ustack,
                uintptr_t sig_kstack, vmm::vmm_ctx *mem_ctx,
                void (*main)(), uint64_t rsp, uint8_t privilege,
                bool assign_tid): ctx(), 
                sig_ctx(), ucontext(), sig_kstack(sig_kstack),
                kstack(kstack), ustack(ustack), mem_ctx(mem_ctx),
                started(0), stopped(0), uptime(0),
                pending_signal(false), dispatch_ready(false), in_syscall(false),
                state(BLOCKED), running(false),

                proc(nullptr), pid(-1), wire(), hook() {
                if (assign_tid) arch::init_thread(this);
                arch::init_context(this, main, rsp, privilege);
            }

            thread(thread *original, vmm::vmm_ctx *ctx, arch::irq_regs *r,
                uintptr_t kstack, uintptr_t sig_kstack):
                sig_ctx(), sig_kstack(sig_kstack), kstack(kstack), mem_ctx(ctx),
                started(0), stopped(0), uptime(0),
                pending_signal(original->pending_signal), dispatch_ready(original->dispatch_ready), in_syscall(original->in_syscall),
                state(BLOCKED), running(false),
                
                proc(nullptr), pid(-1), wire(), hook() { 
                this->sig_ctx.sigmask = original->sig_ctx.sigmask;
                arch::init_thread(this);
                arch::fork_context(original, this, r);
            }
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

        bool load_elf(const char *path, shared_ptr<vfs::fd> fd);
        void set_entry();

        void place_params(char **envp, char **argv, thread *target);
        uint64_t *place_args(uint64_t* location);
        uint64_t *place_auxv(uint64_t *location);
        void load_params(char **argv, char** envp);

        process_env(prs::allocator allocator):
            file(allocator), interp(allocator) {}
    };

    struct process {
        public:
            char name[50];

            prs::allocator allocator;
            vmm::vmm_ctx *mem_ctx;

            prs::vector<thread *, prs::allocator> threads;
            prs::vector<process *, prs::allocator> children;
            prs::vector<process *, prs::allocator> zombies;            
            shared_ptr<vfs::fd_table> fds;
            shared_ptr<vfs::node> cwd;
            shared_ptr<ns::accessor> ns;

            util::spinlock lock;

            uint64_t started;
            uint64_t stopped;

            bool did_exec;

            thread *main_thread;
            process *parent;
            process_group *group;
            session *sess;

            arch::entry_trampoline trampoline;
            signal::sigaction sigactions[SIGNAL_MAX];
            signal::process_ctx sig_ctx;
            util::spinlock sig_lock;
            
            pid_t pid;
            pid_t ppid;
            pid_t pgid;
            pid_t sid;

            uid_t real_uid;
            uid_t effective_uid;
            gid_t saved_uid;

            uid_t real_gid;
            uid_t effective_gid;
            gid_t saved_gid;

            mode_t umask;

            uint8_t privilege;

            ssize_t status;
            process_env env; 

            ipc::wire wire;

            void start();
            void kill(int exit_code = 0);

            void suspend();
            void cont();

            void spawn(void (*main)(), uint64_t rsp, uint8_t privilege);
            void add_thread(thread *task);
            void kill_thread(int64_t tid);
            thread *pick_thread(int signum);

            frg::tuple<int, pid_t> waitpid(pid_t pid, thread *waiter, int options);

            process(shared_ptr<ns::accessor> ns): 
                allocator(arena::create_resource()),
                threads(allocator), children(allocator), zombies(allocator), 
                ns(ns),
                lock(), sig_lock(), env(allocator),
                wire() {};
    };

    struct process_group {
        public:
            pid_t pgid;

            pid_t leader_pid;
            process *leader;
            bool is_orphan;

            session *sess;
            prs::vector<process *, prs::allocator> procs;
            size_t process_count;

            process_group(process *leader): pgid(leader->pid), leader_pid(leader->pid), 
                is_orphan(false), sess(nullptr), 
                procs(arena::create_resource()), process_count(1) {
                procs.push_back(leader);
                
                leader->group = this;
            }

            void add_process(process *proc) {
                procs.push_back(proc);
                proc->group = this;
                process_count++;
            }

            void remove_process(process *proc) {
                procs.erase(proc);
                process_count--;
            }
    };

    // TODO: lock these
    struct session {
        public:
            pid_t sid;
            pid_t leader_pgid;
            process *leader;
            prs::vector<process_group *, prs::allocator> groups;
            size_t group_count;

            tty::device *tty;

            session(process *leader, process_group *group): sid(leader->pid), leader_pgid(leader->pid), 
                leader(leader), 
                groups(arena::create_resource()), group_count(1), tty(nullptr) {
                if (!group) {
                    __builtin_unreachable();
                }

                groups.push_back(group);
                group->sess = this;
                leader->sess = this;
            }

            void add_group(process_group *group) {
                groups.push_back(group);
                group->sess = this;
                group_count++;
            }

            void remove_group(process_group *group) {
                groups.erase(group);
                group_count--;
            }            
    };
};

#endif