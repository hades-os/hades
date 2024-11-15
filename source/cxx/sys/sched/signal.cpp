
#include "fs/vfs.hpp"
#include "mm/common.hpp"
#include "mm/mm.hpp"
#include "mm/pmm.hpp"
#include "mm/vmm.hpp"
#include "sys/irq.hpp"
#include "sys/sched/regs.hpp"
#include "sys/sched/wait.hpp"
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
        smp::set_errno(EINVAL);
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
                smp::set_errno(EINVAL);
                sig_queue->sig_lock.irq_release();
                return -1;
        }
    }

    sig_queue->sigmask &= ~(SIGMASK(SIGKILL) | SIGMASK(SIGSTOP));
    sig_queue->sig_lock.irq_release();

    return 0;
}

int sched::signal::do_kill(pid_t pid, int sig) {
    if (!is_valid(sig)) {
        smp::set_errno(EINVAL);
        return -1;
    }

    auto sender = smp::get_process();
    if (pid > 0) {
        process *target = processes[pid];
        if (!target) {
            // TODO: esrch
            smp::set_errno(EINVAL);
            return -1;
        }

        send_process(sender, target, sig);
    } else if (pid == 0 || pid == -1) {
        send_group(sender, sender->group, sig);
    } else {
        auto session = sender->sess;
        auto group = session->groups[pid];
        if (group == nullptr) {
            // TODO: esrch
            smp::set_errno(ESRCH);
            return -1;
        }

        for (size_t i = 0; i < group->procs.size(); i++) {
            auto target = group->procs[i];
            if (!target) {
                continue;
            }

            send_process(target, sender, sig);
        }
    }

    return 0;
}

int sched::signal::wait_signal(thread *task, queue *sig_queue, sigset_t sigmask, sched::timespec *time) {
    if (time) {
        sig_queue->waitq->set_timer(time);
    }

    sig_queue->sig_lock.irq_acquire();
    for (size_t i = 1; i <= SIGNAL_MAX; i++) {
        if (sigmask & SIGMASK(i)) {
            auto signal = &sig_queue->queue[i - 1];
            sig_queue->sigdelivered &= ~SIGMASK(i);

            if (signal->notify_queue == nullptr) {
                signal->notify_queue = frg::construct<ipc::trigger>(memory::mm::heap);
                signal->notify_queue->add(sig_queue->waitq);
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

        auto msg = sig_queue->waitq->block(smp::get_thread());
        if (msg == nullptr) {
            return -1;
        }

        frg::destruct(memory::mm::heap, msg);
        break;
    }

    return 0;
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
        smp::set_errno(EINVAL);
        return false;
    }

    auto sig_queue = &target->sig_queue;

    target->sig_lock.irq_acquire();
    sig_queue->sig_lock.irq_acquire();

    if (sender != nullptr && !check_perms(sender, target)) {
        smp::set_errno(EPERM);
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
    signal->notify_queue = frg::construct<ipc::trigger>(memory::mm::heap);
    signal->notify_queue->add(sig_queue->waitq);
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

void sigreturn_kill(sched::process *proc, ssize_t status) {
    proc->kill(status);
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
    signal->notify_queue->arise(smp::get_thread());
    frg::destruct(memory::mm::heap, signal->notify_queue);

    sig_queue->sig_lock.irq_release();

    auto regs = &task->sig_context.regs;
    task->reg = *regs;

    memory::pmm::free((void *) task->sig_context.stack);

    if (block_signals) {
        task->state = sched::thread::READY;
        proc->block_signals = true;
    }

    smp::get_locals()->ustack = task->ustack;
    smp::get_locals()->kstack = task->kstack;

    if (regs->cs & 0x3) {
        io::swapgs();
    }

    auto iretq_regs = sched::to_irq(regs);

    sched::set_fcw(regs->fcw);
    sched::set_mxcsr(regs->mxcsr);
    sched::load_sse(task->sig_context.sse_region);
    
    sigreturn_exit(&iretq_regs);
}

void sig_default(sched::process *proc, sched::thread *task, int sig) {
    ssize_t status = sched::WSIGNALED_CONSTRUCT(sig);
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
            sigreturn_kill(proc, status);
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

int sched::signal::process_signals(process *proc, sched::regs *r, char *sse_region) {
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

                memcpy(context.sse_region, sse_region, 512);

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

                r->mxcsr = 0x1F80;
                r->fcw = 0x33F;
                memset(task->sse_region, 0, 512);

                task->sig_context = context;

                auto tmp = task->kstack;
                task->kstack = task->sig_kstack;
                task->sig_kstack = tmp;

                task->sig_ustack = task->ustack;

                proc->sig_lock.irq_release();
                sig_queue->sig_lock.irq_release();

                return 0;
            }

            auto stack = (uint64_t) memory::vmm::map(nullptr, 4 * memory::common::page_size, VMM_USER | VMM_WRITE, proc->mem_ctx) + (4 * memory::common::page_size);
            context.stack = stack;

            context.regs = *r;
            memcpy(context.sse_region, sse_region, 512);

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

            task->sig_context = context;

            r->ss = 0x23;
            r->cs = 0x1B;
            r->rsp = stack;
            r->rflags = 0x202;
            r->cr3 = (uint64_t) memory::vmm::cr3(proc->mem_ctx);

            r->mxcsr = 0x1F80;
            r->fcw = 0x33F;
            memset(task->sse_region, 0, 512);

            // [[noreturn]] sigenter_handler(void *handler_rip, bool is_sigaction, int sig, siginfo *info, ucontext_t *ctx)
            if (proc->sigenter_rip) {
                r->rip = proc->sigenter_rip;
                r->rdi = (uint64_t) action->handler.sa_sigaction;
                r->rsi = action->handler.sa_sigaction != nullptr;
                r->rdx = signal->signum;
                r->rcx = (uint64_t) info;
                r->r8 = (uint64_t) uctx;
            } else {
                r->rip = (uint64_t) action->handler.sa_sigaction;
                r->rdi = signal->signum;
                if (action->sa_flags & SA_SIGINFO) {
                    r->rsi = (uint64_t) info;
                    r->rdx = (uint64_t) uctx;
                }
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