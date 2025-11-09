#ifndef SCHED_HPP
#define SCHED_HPP

#include "mm/vmm.hpp"
#include <cstddef>
#include <cstdint>
#include <frg/hash.hpp>
#include <frg/hash_map.hpp>
#include <frg/rbtree.hpp>
#include <frg/vector.hpp>
#include <mm/mm.hpp>
#include <sys/irq.hpp>
#include <util/lock.hpp>

namespace sched {
    struct regs {
        uint64_t rax, rbx, rcx, rdx, rbp, rdi, rsi, r8, r9, r10, r11, r12, r13, r14, r15;
        uint64_t rsp, rip;
        
        uint64_t ss, cs, fs;
        uint64_t rflags;
        uint64_t cr3;
    };

    inline volatile size_t uptime;

    constexpr size_t PIT_FREQ = 1000;

    class thread;
    class process;

    void sleep(size_t time);
    void init();

    void send_ipis();

    void init_idle();

    void init_syscalls();
    void init_locals();

    void init_bsp();
    void init_ap();

    int64_t start_thread(thread *task);
    thread *create_thread(void (*main)(), uint64_t rsp, memory::vmm::vmm_ctx *ctx, uint8_t privilege);
    void kill_thread(int64_t tid);

    void add_child(thread *task, int64_t pid);

    process *create_process(char *name, memory::vmm::vmm_ctx *ctx);

    int64_t pick_task();
    void swap_task(irq::regs *r);

    void tick_bsp(irq::regs *r);

    class auxv {
        int64_t type;
        union {
            long val;
            void *ptr;
            void (*fun)();
        } data;
    };

    class thread_env {
        public:
            char **argv;
            char **env;
            auxv *auxvs;

            int argc;
            int envc;
            int auxc;
    };

    class thread {
        public:
            regs reg;

            size_t kstack;
            size_t ustack;

            memory::vmm::vmm_ctx *mem_ctx;

            uint64_t started;
            uint64_t stopped;
            uint64_t uptime;

            enum state {
                READY,
                RUNNING,
                SLEEP,
                BLOCKED,
                WAIT
            };

            uint8_t state;
            int64_t cpu;

            int64_t tid;
            size_t pid;
            process *parent;
            
            uint8_t privilege;
            bool running;
            thread_env env;
    };

    class process {
        public:
            char name[50];

            frg::vector<thread *, memory::mm::heap_allocator> threads;

            size_t pid;
            frg::vector<size_t, memory::mm::heap_allocator> fds;

            size_t brk;

            size_t uid;
            size_t gid;
            size_t ppid;

            uint64_t perms;
    };

    struct [[gnu::packed]] thread_info {
        uint64_t meta_ptr;

        int errno;
        int64_t tid;

        size_t started;
        size_t stopped;
        size_t uptime;
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