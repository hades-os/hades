#ifndef ARCH_TYPES_HPP
#define ARCH_TYPES_HPP

#include <cstddef>
#include <util/types.hpp>
#include <sys/sched/time.hpp>

namespace vfs {
    struct fd;
}

namespace sched {
    struct process_env;
    class process;
    class thread;

    namespace signal {
        struct ucontext;
        struct signal;
        struct sigaction;
    }
}

namespace vmm {
    class vmm_ctx;
}

namespace arch {
    void *copy_to_user(void *dst, const void *src, size_t length);
    void *copy_from_user(void *dst, const void *src, size_t length);

    struct [[gnu::packed]] irq_regs;
    struct [[gnu::packed]] sched_regs;
    struct thread_ctx;

    irq_regs sched_to_irq(sched_regs *regs);
    sched_regs irq_to_sched(irq_regs *regs);

    constexpr size_t IRQ0 = 0;

    using irq_fn = void(*)(size_t irq, irq_regs *r, void *private_data);
    struct irq_handler {
        irq_fn fn;
        void *private_data;
    };

    void init_irqs();
    void init_features();

    void irq_on();
    void irq_off();
    bool get_irq_state();

    void route_irq(size_t irq, size_t vector);
    void install_irq(size_t irq, irq_fn handler, void *private_data);

    void init_context(sched::thread *task, void (*main)(), uint64_t rsp, uint8_t privilege);
    void copy_context(sched::thread *original, sched::thread *task);

    void save_context(irq_regs *r, sched::thread *task);
    void rstor_context(sched::thread *task, irq_regs *r);

    void init_default_sigreturn(sched::thread *task, arch::thread_ctx *ctx, 
        sched::signal::signal *signal, sched::signal::ucontext *context);
    void init_user_sigreturn(sched::thread *task, arch::thread_ctx *ctx, 
        sched::signal::signal *signal, sched::signal::sigaction *action,
        sched::signal::ucontext *context);

    void cleanup_vmm_ctx(sched::process *process);

    void init_sched();
    void tick();
    void init_smp();

    void stop_thread(sched::thread *task);
    void stop_all_cpus();

    int do_futex(uintptr_t vaddr, int op, int expected, sched::timespec *timeout);

    sched::process *get_process();
    sched::thread *get_thread();

    void set_process(sched::process *process);
    void set_thread(sched::thread *task);

    tid_t get_tid();
    tid_t get_idle();
    pid_t get_pid();

    void set_errno(int errno);

    namespace loader {
        bool load_elf(const char *path, vfs::fd *fd, sched::process_env *env);
        void set_entry(sched::process_env *env);
        void place_params(char **envp, char **argv, sched::thread *task, sched::process_env *env);
        uint64_t *place_args(uint64_t* location, sched::process_env *env);
        uint64_t *place_auxv(uint64_t *location, sched::process_env *env);
        void load_params(char **argv, char** envp, sched::process_env *env);
    }

    void init_timer();
    void add_timer(sched::timer *timer);
};

#endif
