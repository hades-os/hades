
#include "mm/common.hpp"
#include "mm/mm.hpp"
#include "mm/pmm.hpp"
#include "mm/vmm.hpp"
#include "sys/irq.hpp"
#include "sys/sched/regs.hpp"
#include "sys/smp.hpp"
#include "util/io.hpp"
#include "util/log/log.hpp"
#include "util/string.hpp"
#include <cstddef>
#include <cstdint>
#include <sys/sched/signal.hpp>
#include <sys/sched/sched.hpp>

extern "C" {
    extern void sigreturn_exit(irq::regs *r);
}

int sched::signal::do_sigaction(process *proc, int sig, sched::signal::sigaction *act, sigaction *old) {
    if (!is_valid(sig) || (sig == SIGKILL || sig == SIGSTOP)) {
        // TODO: errno
        return -1;
    }

    auto sig_queue = &proc->sig_queue;
    proc->sig_lock.irq_acquire();

    auto cur_action = &proc->sigactions[sig - 1];
    if (old) {
        *old = *cur_action;
    }

    if (act) {
        *cur_action = *act;
        cur_action->sa_mask &= ~(SIGMASK(SIGKILL) | SIGMASK(SIGSTOP));

        sig_queue->sig_lock.irq_acquire();

        if (act->handler.sa_sigaction == SIG_IGN && sig_queue->sigpending & (1ull << sig)) {
            sig_queue->sigpending &= ~(1 << sig);
        }

        sig_queue->sig_lock.irq_release();
    }

    proc->sig_lock.irq_release();

    return 0;
}

void sched::signal::do_sigpending(process *proc, sigset_t *set) {
    auto sig_queue = &proc->sig_queue;
    sig_queue->sig_lock.irq_acquire();
    *set = sig_queue->sigpending;
    sig_queue->sig_lock.irq_release();
}

int sched::signal::do_sigprocmask(process *proc, int how, sigset_t *set, sigset_t *oldset) {
    auto sig_queue = &proc->sig_queue;
    sig_queue->sig_lock.irq_acquire();

    if (oldset) {
        *oldset = sig_queue->sigmask;
    }

    if (set) {
        switch (how) {
            case SIG_BLOCK:
                sig_queue->sigmask |= *set;
                break;
            case SIG_UNBLOCK:
                sig_queue->sigmask &= ~(*set);
                break;
            case SIG_SETMASK:
                sig_queue->sigmask = *set;
                break;
            default:
                // TODO: errno
                sig_queue->sig_lock.irq_release();
                return -1;
        }
    }

    sig_queue->sigmask &= ~(SIGMASK(SIGKILL) | SIGMASK(SIGSTOP));
    sig_queue->sig_lock.irq_release();

    return 0;
}

int sched::signal::wait_signal(thread *task, queue *sig_queue, sigset_t sigmask, sched::timespec *time) {
    if (time) {
        sig_queue->mail->set_timer(time);
    }

    sig_queue->sig_lock.irq_acquire();
    for (size_t i = 1; i <= SIGNAL_MAX; i++) {
        if (sigmask & SIGMASK(i)) {
            auto signal = &sig_queue->queue[i - 1];
            sig_queue->sigdelivered &= ~SIGMASK(i);

            if (signal->notify_queue == nullptr) {
                signal->notify_queue = sig_queue->mail->make_port();
            }
        }
    }

    sig_queue->sig_lock.irq_release();
    for (;;) {
        for (size_t i = 1; i <= SIGNAL_MAX; i++) {
            if (sig_queue->sigdelivered & SIGMASK(i)) {
                sig_queue->sigdelivered &= ~SIGMASK(i);
                return 0;
            }
        }

        auto msg = sig_queue->mail->recv(true, -1, task);
        if (msg == nullptr) {
            return -1;
        }

        frg::destruct(memory::mm::heap, msg);
        break;
    }
}

bool sched::signal::check_perms(process *sender, process *target) {
    if (sender->real_uid == 0 || sender->effective_uid == 0) {
        return true;
    }

    if (sender->real_uid == target->real_uid || sender->real_uid == target->effective_uid) {
        return true;
    }

    if (sender->effective_uid == target->real_uid || sender->effective_uid == target->effective_uid) {
        return true;
    }

    return false;
}

bool sched::signal::is_valid(int sig) {
    if (sig < 1 || sig > SIGNAL_MAX) {
        return false;
    }

    return true;
}

bool sched::signal::send_process(process *sender, process *target, int sig) {
    if (!is_valid(sig) && sig != 0) {
        // TODO: errno
        return false;
    }

    auto sig_queue = &target->sig_queue;

    target->sig_lock.irq_acquire();
    sig_queue->sig_lock.irq_acquire();

    if (sender != nullptr && !check_perms(sender, target)) {
        // TODO: errno
        sig_queue->sig_lock.irq_release();
        target->sig_lock.irq_release();
        return false;
    }

    if (sig != SIGKILL && sig != SIGSTOP) {
        if (target->sigactions[sig - 1].handler.sa_sigaction == SIG_IGN) {
            sig_queue->sig_lock.irq_release();
            target->sig_lock.irq_release();
            return true;
        }
    }

    auto signal = &sig_queue->queue[sig - 1];

    signal->ref = 1;
    signal->signum = sig;
    signal->info = frg::construct<siginfo>(memory::mm::heap);
    signal->notify_queue = sig_queue->mail->make_port();
    sig_queue->sigpending |= SIGMASK(sig);
    sig_queue->sig_lock.irq_release();
    target->sig_lock.irq_release();
    return true;
}

bool sched::signal::send_group(process *sender, process_group *target, int sig) {
    for (size_t i = 0; i < target->procs.size(); i++) {
        if (target->procs[i] == nullptr) continue;

        auto proc = target->procs[i];
        send_process(sender, proc, sig);
    }

    return true;
}

void sigreturn_kill(sched::process *proc) {
    proc->kill();
    for (;;) {
        asm volatile("hlt");
    }
}

void sigreturn_default(sched::process *proc, sched::thread *task, bool block_signals) {
    irq::off();

    auto sig_queue = &proc->sig_queue;
    sig_queue->sig_lock.irq_acquire();

    auto signal = &sig_queue->queue[task->sig_context.signum - 1];
    sig_queue->sigdelivered |= SIGMASK(task->sig_context.signum);
    signal->notify_queue->post({});

    sig_queue->sig_lock.irq_release();

    auto regs = &task->sig_context.regs;
    task->reg = *regs;

    if (block_signals) {
        task->state = sched::thread::READY;
        proc->block_signals = true;
    }

    irq::regs r{
        .r15 = regs->r15,
        .r14 = regs->r14,
        .r13 = regs->r13,
        .r12 = regs->r12,
        .r11 = regs->r11,
        .r10 = regs->r10,
        .r9 = regs->r9,
        .r8 = regs->r8,
        .rsi = regs->rsi,
        .rdi = regs->rdi,
        .rbp = regs->rbx,
        .rdx = regs->rdx,
        .rcx = regs->rcx,
        .rbx = regs->rbx,
        .rax = regs->rax,

        .int_no = 0,
        .err = 0,

        .rip = regs->rip,
        .cs = regs->cs,
        .rflags = regs->rflags,
        .rsp = regs->rsp,
        .ss = regs->ss,
    };

    if (regs->cs & 0x3) {
        io::swapgs();
    }

    sigreturn_exit(&r);
}

void sig_default(sched::process *proc, sched::thread *task, int sig) {
    switch (sig) {
		case SIGHUP:
		case SIGINT:
		case SIGQUIT:
		case SIGILL:
		case SIGTRAP:
		case SIGBUS:
		case SIGFPE:
		case SIGKILL:
		case SIGUSR1:
		case SIGSEGV:
		case SIGUSR2:
		case SIGPIPE:
		case SIGALRM:
		case SIGSTKFLT:
		case SIGXCPU:
		case SIGXFSZ:
		case SIGVTALRM:
		case SIGPROF:
		case SIGSYS:
            sigreturn_kill(proc);
            break;
        case SIGSTOP:
        case SIGTTIN:
        case SIGTTOU:
        case SIGTSTP:
            proc->suspend();
            break;
        case SIGCONT:
            proc->cont();
            break;
        case SIGCHLD:
        case SIGWINCH:
            break;
    }

    sigreturn_default(proc, task, false);
}

int sched::signal::process_signals(process *proc, sched::regs *r) {
    auto sig_queue= &proc->sig_queue;
    sig_queue->sig_lock.irq_acquire();
    if (sig_queue->active == false) {
        sig_queue->sig_lock.irq_release();
        return -1;
    }

    if (sig_queue->sigpending == 0) {
        sig_queue->sig_lock.irq_release();
        return -1;
    }

    for (size_t i = 1; i <= SIGNAL_MAX; i++) {
        if ((sig_queue->sigpending & (SIGMASK(i))) && !(sig_queue->sigmask & SIGMASK(i))) {
            auto signal = &sig_queue->queue[i - 1];
            auto action = &proc->sigactions[signal->signum - 1];
            proc->sig_lock.irq_acquire();

            auto task = proc->pick_thread();

            sig_queue->sigpending &= ~(SIGMASK(i));
            if (action->handler.sa_sigaction == SIG_ERR) {
                proc->sig_lock.irq_release();
                sig_queue->sig_lock.irq_release();

                return -1;
            } else if (action->handler.sa_sigaction == SIG_IGN) {
                proc->sig_lock.irq_release();
                continue;
            }

            ucontext context{};

            if (action->handler.sa_sigaction == SIG_DFL) {
                auto stack = memory::pmm::stack(4);

                context.stack = (uint64_t) stack;
                context.regs = *r;
                context.signum = signal->signum;

                memset(r, 0, sizeof(regs));

                r->ss = 0x10;
                r->cs = 0x8;
                r->rsp = context.stack;
                r->rflags = 0x202;

                r->rdi = (uint64_t) proc;
                r->rsi = (uint64_t) task;
                r->rdx = signal->signum;
                r->rip = (uint64_t) sig_default;

                r->cr3 = (uint64_t) memory::vmm::cr3(proc->mem_ctx);
                task->sig_context = context;

                auto tmp = task->kstack;
                task->kstack = task->sig_kstack;
                task->sig_kstack = tmp;

                task->sig_ustack = task->ustack;

                proc->sig_lock.irq_release();
                sig_queue->sig_lock.irq_release();

                return 0;
            }

            auto stack = (uint64_t) memory::vmm::map(nullptr, 4 * memory::common::page_size, VMM_PRESENT | VMM_WRITE | VMM_USER, proc->mem_ctx) + (4 * memory::common::page_size);
            context.stack = stack;
            context.regs = *r;
            context.signum = signal->signum;

            memset(r, 0, sizeof(regs));

            stack -= 128;
            stack &= -1611;
            stack -= sizeof(siginfo);
            siginfo *info = (siginfo *) stack;
            *info = *signal->info;

            info->si_signo = signal->signum;

            stack -= sizeof(sched::regs);
            ucontext *uctx = (ucontext *) stack;
            *uctx = context;

            stack -= sizeof(uint64_t);
            *(uint64_t *) stack = (uint64_t) action->sa_restorer;

            task->sig_context = context;

            r->ss = 0x23;
            r->cs = 0x1B;
            r->rsp = stack;
            r->rflags = 0x202;
            r->rip = (uint64_t) action->handler.sa_sigaction;

            r->rdi = signal->signum;
            r->cr3 = (uint64_t) memory::vmm::cr3(proc->mem_ctx);

            if (action->sa_flags & SA_SIGINFO) {
                r->rsi = (uint64_t) info;
                r->rdx = (uint64_t) uctx;
            }

            auto tmp = task->kstack;
            task->kstack = task->sig_kstack;
            task->sig_kstack = tmp;

            tmp = task->ustack;
            task->ustack = stack;
            task->sig_ustack = tmp;

            proc->sig_lock.irq_release();

            break;
        }
    }

    sig_queue->sig_lock.irq_release();
    return 0;
}

bool sched::signal::is_blocked(process *proc, int sig) {
    if (!is_valid(sig)) {
        return true;
    }

    proc->sig_queue.sig_lock.irq_acquire();
    if (proc->sig_queue.sigmask & SIGMASK(sig)) {
        proc->sig_queue.sig_lock.irq_release();
        return false;        
    }

    proc->sig_queue.sig_lock.irq_release();
    return true;
}

bool sched::signal::is_ignored(process *proc, int sig) {
    if (!is_valid(sig)) {
        return true;
    }

    proc->sig_queue.sig_lock.irq_acquire();
    sigaction *act = &proc->sigactions[sig - 1];
    if (act->handler.sa_sigaction == SIG_IGN) {
        proc->sig_queue.sig_lock.irq_release();
        return true;        
    }

    proc->sig_queue.sig_lock.irq_release();
    return false;
}