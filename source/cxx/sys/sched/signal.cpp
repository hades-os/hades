#include "arch/vmm.hpp"
#include "arch/x86/smp.hpp"
#include "arch/x86/types.hpp"
#include "fs/vfs.hpp"
#include "ipc/evtable.hpp"
#include "mm/common.hpp"
#include "mm/mm.hpp"
#include "mm/pmm.hpp"
#include "mm/vmm.hpp"
#include "util/lock.hpp"
#include <cstddef>
#include <cstdint>
#include <sys/sched/signal.hpp>
#include <sys/sched/sched.hpp>

int sched::signal::do_sigaction(process *proc, thread *task, int sig, sigaction *act, sigaction *old) {
    if (!is_valid(sig) || (sig == SIGKILL || sig == SIGSTOP)) {
        arch::set_errno(EINVAL);
        return -1;
    }

    util::lock_guard sig_guard{proc->sig_lock};

    auto cur_action = &proc->sigactions[sig - 1];
    if (old) {
        *old = *cur_action;
    }

    if (act) {
        *cur_action = *act;
        cur_action->sa_mask &= ~(SIGMASK(SIGKILL) | SIGMASK(SIGSTOP));

        auto proc_ctx = &proc->sig_ctx;
        auto ctx = &task->sig_ctx;

        util::lock_guard proc_guard{proc_ctx->lock};
        util::lock_guard ctx_guard{ctx->lock};

        if (act->handler.sa_sigaction == SIG_IGN && ctx->sigpending & (1ull << sig)) {
            proc_ctx->sigpending &= ~(1 << sig);
            ctx->sigpending &= ~(1 << sig);
        }
    }

    return 0;
}

void sched::signal::do_sigpending(thread *task, sigset_t *set) {
    auto ctx = &task->sig_ctx;
    util::lock_guard ctx_guard{ctx->lock};

    *set = ctx->sigpending;
}

int sched::signal::do_sigprocmask(thread *task, int how, sigset_t *set, sigset_t *oldset) {
    auto ctx = &task->sig_ctx;
    util::lock_guard ctx_guard{ctx->lock};

    if (oldset) {
        *oldset = ctx->sigmask;
    }

    if (set) {
        switch (how) {
            case SIG_BLOCK:
                ctx->sigmask |= *set;
                break;
            case SIG_UNBLOCK:
                ctx->sigmask &= ~(*set);
                break;
            case SIG_SETMASK:
                ctx->sigmask = *set;
                break;
            default:
                arch::set_errno(EINVAL);
                return -1;
        }
    }

    ctx->sigmask &= ~(SIGMASK(SIGKILL) | SIGMASK(SIGSTOP));
    return 0;
}

int sched::signal::do_kill(pid_t pid, int sig) {
    if (!is_valid(sig)) {
        arch::set_errno(EINVAL);
        return -1;
    }

    auto sender = arch::get_process();
    if (pid > 0) {
        process *target = sched::get_process(pid);
        if (!target) {
            // TODO: esrch
            arch::set_errno(EINVAL);
            return -1;
        }

        send_process(sender, target, sig);
    } else if (pid == 0 || pid == -1) {
        send_group(sender, sender->group, sig);
    } else {
        auto session = sender->sess;
        auto group = session->groups[pid];

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

int sched::signal::wait_signal(thread *task, sigset_t sigmask, sched::timespec *time) {
    auto ctx = &task->sig_ctx;
    ctx->lock.lock();

    for (size_t i = 1; i <= SIGNAL_MAX; i++) {
        if (sigmask & SIGMASK(i)) ctx->sigdelivered &= ~SIGMASK(i);
    }

    ctx->lock.unlock();
    for (;;) {
        for (size_t i = 1; i <= SIGNAL_MAX; i++) {
            if (ctx->sigdelivered & SIGMASK(i)) {
                ctx->sigdelivered &= ~SIGMASK(i);
                goto done;
            }
        }

        auto [evt, _] = ctx->wire.wait(evtable::SIGNAL, true, time);
        if (evt > 0) {
            return -1;
        }
    }

    done:
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
        arch::set_errno(EINVAL);
        return false;
    }

    auto ctx = &target->sig_ctx;

    util::lock_guard sig_guard{target->sig_lock};
    util::lock_guard guard{ctx->lock};

    if (sender != nullptr && !check_perms(sender, target)) {
        arch::set_errno(EPERM);
        return false;
    }

    if (sig != SIGKILL && sig != SIGSTOP) {
        if (target->sigactions[sig - 1].handler.sa_sigaction == SIG_IGN) {
            return true;
        }
    }

    ctx->sigpending |= SIGMASK(sig);
    return true;
}

bool sched::signal::send_group(process *sender, process_group *target, int sig) {
    for (size_t i = 0; i < target->procs.size(); i++) {
        auto proc = target->procs[i];
        send_process(sender, proc, sig);
    }

    return true;
}

int sched::signal::issue_signals(process *proc) {
    auto proc_ctx = &proc->sig_ctx;
    for (size_t i = 1; i <= SIGNAL_MAX; i++) {
        if (proc_ctx->sigpending & SIGMASK(i)) {
            auto task = proc->pick_thread(i);
            if (task == nullptr) {
                return -1;
            }

            auto task_ctx = &task->sig_ctx;
            auto signal = &task_ctx->queue[i - 1];

            util::lock_guard sig_guard{proc->sig_lock};
            util::lock_guard task_guard{task_ctx->lock};

            proc_ctx->sigpending &= ~SIGMASK(i);
            task_ctx->sigpending |= SIGMASK(i);

            signal->signum = i;
        }
    }

    return 0;
}

int sched::signal::dispatch_signals(process *proc, thread *task) {
    if (task->state == thread::DEAD || task->dispatch_ready
        || task->in_syscall) {
        if (~task->sig_ctx.sigmask & task->sig_ctx.sigpending) task->pending_signal = true;
        return -1;
    }

    for (size_t i = 1; i <= SIGNAL_MAX; i++) {
        if (task->sig_ctx.sigpending & SIGMASK(i)) {
            auto task_ctx = &task->sig_ctx;
            auto signal = &task_ctx->queue[i - 1];

            util::lock_guard sig_guard{proc->sig_lock};
            util::lock_guard task_guard{task_ctx->lock};

            if (task->sig_ctx.sigmask & SIGMASK(i)) {
                continue;
            }

            task->pending_signal = false;
            task_ctx->sigpending &= ~SIGMASK(i);

            auto action = &proc->sigactions[i - 1];
            if (action->handler.sa_sigaction == SIG_ERR) {
                return -1;
            } else if (action->handler.sa_sigaction == SIG_IGN) {
                continue;
            }

            task->dispatch_ready = true;
            if (action->handler.sa_sigaction == SIG_DFL) {
                auto stack = pmm::stack(x86::initialStackSize);

                task->ucontext.stack = (uint64_t) stack;
                
                arch::init_default_sigreturn(task, signal, &task->ucontext);

                auto tmp = task->kstack;
                task->kstack = task->sig_kstack;
                task->sig_kstack = tmp;

                return 0;
            }

            auto stack = (size_t) proc->mem_ctx->stack(nullptr, memory::user_stack_size, vmm::map_flags::USER | vmm::map_flags::WRITE | vmm::map_flags::DEMAND);
            task->ucontext.stack = stack;
            
            arch::init_user_sigreturn(task, signal, action, &task->ucontext);

            auto tmp = task->kstack;
            task->kstack = task->sig_kstack;
            task->sig_kstack = tmp;

            tmp = task->ustack;
            task->ustack = stack;
            task->sig_ustack = tmp;

            return 0;
        }
    }

    return -1;
}

int sched::signal::process_signals(process *proc, thread *task) {
    auto proc_ctx = &proc->sig_ctx;

    util::lock_guard proc_guard{proc_ctx->lock};

    issue_signals(proc);
    if (dispatch_signals(proc, task) < 0) {
        return -1;
    }
    
    return 0;
}

bool sched::signal::is_blocked(thread *task, int sig) {
    if (!is_valid(sig)) {
        return true;
    }

    auto ctx = &task->sig_ctx;
    util::lock_guard ctx_guard{ctx->lock};

    if (ctx->sigmask & SIGMASK(sig)) {
        return false;
    }

    return true;
}

bool sched::signal::is_ignored(process *proc, int sig) {
    if (!is_valid(sig)) {
        return true;
    }

    auto ctx = &proc->sig_ctx;
    util::lock_guard ctx_guard{ctx->lock};

    sigaction *act = &proc->sigactions[sig - 1];
    if (act->handler.sa_sigaction == SIG_IGN) {
        return true;
    }

    return false;
}