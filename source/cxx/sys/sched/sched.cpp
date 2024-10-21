#include "fs/vfs.hpp"
#include "mm/common.hpp"
#include "sys/sched/mail.hpp"
#include "sys/sched/signal.hpp"
#include "util/elf.hpp"
#include "util/string.hpp"
#include <cstddef>
#include <cstdint>
#include <frg/allocation.hpp>
#include <frg/vector.hpp>
#include <mm/mm.hpp>
#include <mm/pmm.hpp>
#include <mm/vmm.hpp>
#include <sys/irq.hpp>
#include <sys/sched/sched.hpp>
#include <sys/smp.hpp>
#include <sys/x86/apic.hpp>
#include <util/io.hpp>
#include <util/log/log.hpp>
#include <util/lock.hpp>

sched::regs default_kernel_regs{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x10, 0x8, 0, 0x202, 0 };
sched::regs default_user_regs{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x23, 0x1B, 0, 0x202, 0 };

util::lock sched::sched_lock{};
extern "C" {
    extern void syscall_enter();
}

static void _idle() {
    while (1) { asm volatile("hlt"); };
}

static void tick_ap(irq::regs *r) {
    sched::swap_task(r);
}

void sched::tick_bsp(irq::regs *r) {
    swap_task(r);
}

void sched::retick() {
    irq::on();
    apic::lapic::ipi(smp::get_locals()->lid, 34);
}

void sched::send_ipis() {
    auto info = smp::get_locals();
    for (auto cpu : smp::cpus) {
        if (info->lid == cpu->lid) {
            continue;
        }

        apic::lapic::ipi(cpu->lid, 253);
    }
}

void sched::sleep(size_t time) {
    volatile uint64_t final_time = uptime + (time * (PIT_FREQ / 1000));

    final_time += 1;
    while (uptime < final_time) {  }
}

void sched::init() {
    irq::add_handler(&tick_ap, 253);
    init_locals();
}

void sched::init_syscalls() {
    io::wrmsr(EFER, io::rdmsr<uint64_t>(EFER) | (1 << 0));
    io::wrmsr(STAR, (0x18ull << 48 | 0x8ull << 32));
    io::wrmsr(LSTAR, (uintptr_t) syscall_enter);
    io::wrmsr(SFMASK, (1 << 9));
}

void sched::init_locals() {
    init_syscalls();
    uint64_t idle_rsp = (uint64_t) memory::pmm::stack(1);

    auto idle_task = create_thread(_idle, idle_rsp, memory::vmm::common::boot_ctx, 0);
    idle_task->state = thread::BLOCKED;
    auto idle_tid = idle_task->start();

    smp::get_locals()->idle_tid = idle_tid;
    smp::get_locals()->tid = idle_tid;
}

sched::thread *sched::create_thread(void (*main)(), uint64_t rsp, memory::vmm::vmm_ctx *ctx, uint8_t privilege) {
    thread *task = frg::construct<thread>(memory::mm::heap);

    task->kstack = (size_t) memory::pmm::stack(4);

    task->sig_kstack = (size_t) memory::pmm::stack(4);
    task->sig_ustack = (size_t) memory::pmm::stack(4);
    if (privilege == 3) {
        task->reg = default_user_regs;
    } else {
        task->reg = default_kernel_regs;
    }

    task->reg.rip = (uint64_t) main;
    task->ustack = rsp;
    task->reg.rsp = rsp;
    task->privilege = privilege;
    task->mem_ctx = ctx;
    task->pid = -1;

    task->reg.cr3 = (uint64_t) memory::vmm::cr3(ctx);
    task->state = thread::READY;
    task->cpu = -1;

    task->env.envc = 0;
    task->env.argc = 0;
    task->env.env = nullptr;
    task->env.argv = nullptr;
    task->proc = nullptr;

    return task;
}

sched::process *sched::create_process(char *name, void (*main)(),
    uint64_t rsp, memory::vmm::vmm_ctx *ctx, uint8_t privilege) {
    process *proc = frg::construct<process>(memory::mm::heap);

    proc->threads = frg::vector<thread *, memory::mm::heap_allocator>();
    proc->children = frg::vector<process *, memory::mm::heap_allocator>();
    proc->zombies = frg::vector<process *, memory::mm::heap_allocator>();
    proc->fds = vfs::make_table();

    proc->main_thread = create_thread(main, rsp, ctx, privilege);
    proc->main_thread->proc = proc;
    proc->main_thread->pid = proc->pid;
    proc->threads.push_back(proc->main_thread);

    proc->env = process_env{};
    proc->env.proc = proc;
    proc->mem_ctx = ctx;
    proc->parent = nullptr;
    proc->group = nullptr;
    proc->sess = nullptr;

    proc->lock = util::lock();
    proc->sig_lock = util::lock();
    proc->block_signals = false;
    proc->sig_queue = signal::queue{};
    proc->sig_queue.mail = frg::construct<ipc::mailbox>(memory::mm::heap, proc);
    proc->sig_queue.active = true;
    // default sigactions
    for (size_t i = 0; i < SIGNAL_MAX; i++) {
        signal::sigaction *sa = &proc->sigactions[i];
        sa->handler.sa_sigaction = (void (*)(int, signal::siginfo *, void *)) SIG_DFL;
    }

    proc->mail = frg::construct<ipc::mailbox>(memory::mm::heap, proc);
    proc->parent_port = nullptr;
    proc->privilege = privilege;

    proc->status = {
        .term_signal = 0,
        .stop_signal = 0,

        .val = process::RUNNING
    };

    return proc;
}

sched::thread *sched::fork(thread *original, memory::vmm::vmm_ctx *ctx) {
    thread *task = frg::construct<thread>(memory::mm::heap);

    task->kstack = (size_t) memory::pmm::stack(4);
    task->sig_kstack = (size_t) memory::pmm::stack(4);

    task->sig_ustack = original->sig_ustack;
    task->ustack = original->ustack;

    task->reg = original->reg;
    task->privilege = original->privilege;
    task->mem_ctx = ctx;

    task->reg.cr3 = (uint64_t) memory::vmm::cr3(ctx);
    task->state = thread::READY;
    task->cpu = -1;

    task->env.envc = original->env.envc;
    task->env.argc = original->env.argc;
    task->env.env = original->env.env;
    task->env.argv = original->env.argv;
    task->proc = nullptr;

    return task;
}

sched::process *sched::fork(process *original, thread *caller) {
    sched_lock.irq_acquire();

    process *proc = frg::construct<process>(memory::mm::heap);

    proc->threads = frg::vector<thread *, memory::mm::heap_allocator>();
    proc->children = frg::vector<process *, memory::mm::heap_allocator>();
    proc->zombies = frg::vector<process *, memory::mm::heap_allocator>();
    proc->fds = vfs::copy_table(original->fds);

    proc->parent = original;
    proc->ppid = original->ppid;
    proc->sig_lock = util::lock();
    proc->sig_queue = signal::queue{};
    proc->sig_queue.mail = frg::construct<ipc::mailbox>(memory::mm::heap, proc);
    proc->sig_queue.active = true;
    proc->sig_queue.sigmask = original->sig_queue.sigmask;
    memcpy(&proc->sigactions, &original->sigactions, SIGNAL_MAX * sizeof(signal::sigaction));

    proc->user_fs = original->user_fs;
    proc->user_gs = original->user_gs;
    proc->env = original->env;
    proc->env.proc = proc;
    proc->mem_ctx = (memory::vmm::vmm_ctx *) memory::vmm::fork(original->mem_ctx);

    proc->main_thread = fork(caller, proc->mem_ctx);
    proc->main_thread->proc = proc;
    proc->threads.push_back(proc->main_thread);

    proc->sess = original->sess;
    if (original->group) {
        proc->group = original->group;
        original->group->procs.push_back(proc);
    }

    proc->lock = util::lock{};
    proc->status.val = process::RUNNING;

    proc->real_uid = original->real_uid;
    proc->effective_uid = original->effective_uid;
    proc->saved_gid = original->saved_gid;

    proc->mail = frg::construct<ipc::mailbox>(memory::mm::heap, proc);
    proc->parent_port = original->mail->make_port();

    original->children.push_back(proc);

    sched_lock.irq_release();

    return proc;
}

// TODO: free if fails
bool sched::exec(thread *target, const char *path, char **argv, char **envp) {
    size_t envc = 0;
    for (;; envc++) {
        if (envp[envc] == nullptr) {
            break;
        }
    }

    size_t argc = 0;
    for (;; argc++) {
        if (argv[argc] == nullptr) {
            break;
        }
    }

    auto fd = vfs::open(path, target->proc->fds, 0, 0);
    if (!fd) {
        // TODO: errno
        return false;
    }

    for (size_t i = 0; i < target->proc->threads.size(); i++) {
        auto task = target->proc->threads[i];
        if (task == nullptr) continue;
        if (task->tid == target->tid) continue;

        task->kill();
    }

    target->proc->main_thread = target;
    target->stop();

    memory::vmm::destroy(target->proc->mem_ctx);

    target->proc->mem_ctx = (memory::vmm::vmm_ctx *) memory::vmm::create();
    memory::vmm::change(target->proc->mem_ctx);

    auto res = target->proc->env.load_elf(path);
    if (!res) {
        return false;
    }

    target->ustack = (uint64_t) memory::vmm::map(nullptr, 4 * memory::common::page_size, VMM_PRESENT | VMM_USER | VMM_WRITE | VMM_MANAGED, (void *) target->proc->mem_ctx) + (4 * memory::common::page_size);
    target->reg.cr3 = (uint64_t) memory::vmm::cr3(target->proc->mem_ctx);
    target->reg.rsp = target->ustack;

    target->reg.rip = 0;
    target->reg.cs = 0x1B;
    target->reg.ss = 0x23;
    target->reg.rflags = 0x202;

    target->proc->env.place_params(envp, argv, target);
    
    vfs::delete_table(target->proc->fds);
    target->proc->fds = vfs::make_table();

    for (size_t i = 0; i < SIGNAL_MAX; i++) {
        signal::sigaction *act = &target->proc->sigactions[i];
        auto handler = act->handler.sa_handler;
        memset(act, 0, sizeof(signal::sigaction));

        if (handler == SIG_IGN) {
            act->handler.sa_handler = (void(*)(int)) SIG_IGN;
        } else {
            act->handler.sa_handler = (void(*)(int)) SIG_DFL;
        }
    }

    target->cont();
    return true;
}

sched::thread *sched::process::pick_thread() {
    for (size_t i = 0; i < threads.size(); i++) {
        if (threads[i] == nullptr) continue;
        if (threads[i]->state == thread::DEAD || threads[i]->state == thread::BLOCKED) continue;

        return threads[i];
    }

    return nullptr;
}

void sched::process_env::place_params(char **envp, char **argv, thread *target) {
    load_params(argv, envp);

    uint64_t *location = (uint64_t *) target->ustack;
    uint64_t args_location = (uint64_t) location;

    location = place_args(location);
    location = place_auxv(location);

    *(--location) = 0;
    location -= params.envc;
    for (size_t i = 0; i < params.envc; i++) {
        args_location -= strlen(params.envp[i]) + 1;
        location[i] = args_location;
    }

    *(--location) = 0;
    location -= params.argc;
    for (size_t i = 0; i < params.argc; i++) {
        args_location -= strlen(params.argv[i]) + 1;
        location[i] = args_location;
    }

    *(--location) = params.argc;
    target->reg.rsp = (uint64_t) location;
}

//TODO : cleanup
bool sched::process_env::load_elf(const char *path) {
    auto fd = vfs::open(path, proc->fds, 0, 0);
    if (!fd) {
        return false;
    }

    file.ctx = proc->mem_ctx;
    auto res = file.init(fd);
    if (!res) return false;

    file.load_aux();
    file.load();

    entry = file.aux.at_entry;
    has_interp = file.load_interp(&interp_path);

    vfs::close(fd);

    if (has_interp) {
        fd = vfs::open(interp_path, nullptr, 0, 0);
        if (!fd) {
            return -1;
        }

        interp.ctx = proc->mem_ctx;
        interp.load_offset = 0x40000000;
        interp.fd = fd;

        res = interp.init(fd);
        if (!res) return false;

        interp.load_aux();
        interp.load();

        entry = interp.aux.at_entry;

        vfs::close(fd);
    }

    file_path = (char *) kmalloc(strlen(path) + 1);
    strcpy(file_path, path);

    is_loaded = true;

    return true;
}

uint64_t *sched::process_env::place_args(uint64_t *location) {
    for (size_t i = 0; i < params.envc; i++) {
        location = (uint64_t *)((char *) location - (strlen(params.envp[i]) + 1));
        strcpy((char *) location, params.envp[i]);
    }

    for (size_t i = 0; i < params.argc; i++) {
        location = (uint64_t *)((char *) location - (strlen(params.argv[i]) + 1));
        strcpy((char *) location, params.argv[i]);
    }

    location = (uint64_t *) ((uint64_t) location & -1611);

    if ((params.argc + params.envc + 1) & 1) {
        location--;
    }

    return location;
}

uint64_t *sched::process_env::place_auxv(uint64_t *location) {
    location -= 10;

    location[0] = ELF_AT_PHNUM;
    location[1] = file.aux.at_phnum;

    location[2] = ELF_AT_PHENT;
    location[3] = file.aux.at_phent;

    location[4] = ELF_AT_PHDR;
    location[5] = file.aux.at_phdr;

    location[6] = ELF_AT_ENTRY;
    location[7] = file.aux.at_entry;

    location[8] = 0; location[9] = 0;

    return location;
}

void sched::process_env::load_params(char **argv, char **envp) {
    for (;; params.envc++) {
        if (envp[params.envc] == nullptr) break;
    }

    for (;; params.argc++) {
        if (argv[params.argc] == nullptr) break;
    }

    params.argv = (char **) kmalloc(sizeof (char *) * params.argc);
    params.envp = (char **) kmalloc(sizeof (char *) * params.envc);

    for (size_t i = 0; i < params.argc; i++) {
        params.argv[i] = (char *) kmalloc(strlen(argv[i] + 1));
        strcpy(params.argv[i], argv[i]);
    }

    for (size_t i = 0; i < params.envc; i++) {
        params.envp[i] = (char *) kmalloc(strlen(envp[i] + 1));
        strcpy(params.envp[i], envp[i]);
    }
}

sched::process *sched::find_process(pid_t pid) {
    for (size_t i = 0; i < processes.size(); i++) {
        process *proc = processes[i];
        if (proc->pid == pid) {
            return proc;
        }
    }

    return nullptr;
}

int64_t sched::process::start() {
    sched_lock.irq_acquire();

    processes.push_back(this);
    pid_t new_pid = processes.size() - 1;
    this->pid = new_pid;
    main_thread->pid = new_pid;
    this->status.val = RUNNING;

    sched_lock.irq_release();

    main_thread->start();

    return pid;
}

void sched::process::kill() {
    sched_lock.irq_acquire();
    vfs::delete_table(this->fds);

    for (size_t i = 0; i < this->threads.size(); i++) {
        auto task = this->threads[i];
        if (task == nullptr) continue;
        if (task->tid == smp::get_locals()->tid) {
            this->main_thread = task;
            continue;
        };

        while (task->state == thread::BLOCKED) { retick(); }
        task->state = thread::DEAD;
        if (task->cpu != -1) {
            auto cpu = task->cpu;
            sched_lock.irq_release();

            apic::lapic::ipi(cpu, 253);
            while (task->cpu != -1) { asm volatile("pause"); };

            sched_lock.irq_acquire();
        }

        threads[task->tid] = nullptr;
        frg::destruct(memory::mm::heap, task);
    }

    auto old_ctx = this->mem_ctx;
    this->mem_ctx = (memory::vmm::vmm_ctx *) memory::vmm::boot();
    this->main_thread->mem_ctx = this->mem_ctx;
    this->main_thread->reg.cr3 = (uint64_t) memory::vmm::cr3(memory::vmm::boot());
    memory::vmm::destroy(old_ctx);
    signal::send_process(nullptr, this->parent, SIGCHLD);

    for (size_t i = 0; i < children.size(); i++) {
        auto child = children[i];
        if (child == nullptr) continue;
        
        frg::destruct(memory::mm::heap, child->parent_port);
        child->parent_port = parent->mail->make_port();

        child->parent = parent;
        child->ppid = parent->ppid;
        parent->children.push(child);

        children[i] = nullptr;
    }

    for (size_t i = 0; i < zombies.size(); i++) {
        auto zombie = zombies[i];
        if (zombie == nullptr) continue;

        frg::destruct(memory::mm::heap, zombie->parent_port);
        zombie->parent_port = parent->mail->make_port();

        zombie->parent = parent;
        zombie->ppid = parent->ppid;
        parent->zombies.push(zombie);

        zombies[i] = nullptr;
    }

    parent->children[parent->find_child(this)] = nullptr;
    parent->zombies.push_back(this);

    status.val = TERMINATED;
    parent_port->post({
        .who = {
            .task = main_thread,
            .proc = this,
            .pid = this->pid,
            .tid = main_thread->tid
        },

        .what = messages::KILLED
    });

    sched_lock.irq_release();
}

void sched::process::suspend() {
    for (size_t i = 0; i < threads.size(); i++) {
        auto task = threads[i];
        if (task == nullptr) continue;

        task->stop();
    }

    status.val = STOPPED;
    signal::send_process(nullptr, parent, SIGCHLD);
    parent_port->post({
        .who = {
            .task = main_thread,
            .proc = this,
            .pid = this->pid,
            .tid = main_thread->tid
        },

        .what = messages::SUSPEND
    });
}

void sched::process::cont() {
    for (size_t i = 0; i < threads.size(); i++) {
        auto task = threads[i];
        if (task == nullptr) continue;

        task->cont();
    }

    status.val = RUNNING;
    signal::send_process(nullptr, parent, SIGCHLD);
    parent_port->post({
        .who = {
            .task = main_thread,
            .proc = this,
            .pid = this->pid,
            .tid = main_thread->tid
        },

        .what = messages::STARTED
    });
}

int64_t sched::thread::start() {
    sched_lock.irq_acquire();

    threads.push_back(this);
    tid_t tid = threads.size() - 1;
    this->tid = tid;

    if (this->proc && this->proc->status.val == process::STOPPED) {
        this->proc->status.val = process::RUNNING;
    }

    sched_lock.irq_release();

    return tid;
}

void sched::thread::stop() {
    sched_lock.acquire();

    this->state = thread::BLOCKED;
    if (this->cpu != -1) {
        auto cpu = this->cpu;
        sched_lock.release();

        apic::lapic::ipi(cpu, 253);
        while (this->cpu != -1) { asm volatile("pause"); };

        sched_lock.acquire();
    }

    sched_lock.release();
}

void sched::thread::cont() {
    sched_lock.acquire();
    this->state = thread::READY;
    sched_lock.release();
}

int64_t sched::thread::kill() {
    sched_lock.acquire();

    this->state = thread::DEAD;
    if (this->cpu != -1) {
        auto cpu = this->cpu;
        sched_lock.release();

        apic::lapic::ipi(cpu, 253);
        while (this->cpu != -1) { asm volatile("pause"); };

        sched_lock.acquire();
    }

    threads[tid] = (sched::thread *) 0;
    sched_lock.release();

    return this->tid;
}


void sched::process::add_thread(thread *task) {
    sched_lock.irq_acquire();

    task->proc = this;
    task->pid = this->pid;
    this->threads.push(task);

    sched_lock.irq_release();
}

size_t sched::process::find_child(sched::process *proc) {
    for (size_t i = 0; i < children.size(); i++) {
        if (children[i] && (proc->pid == children[i]->pid)) {
            return i;
        }
    }

    return -1;
}

size_t sched::process::find_zombie(sched::process *proc) {
    for (size_t i = 0; i < zombies.size(); i++) {
        if (zombies[i] && (proc->pid == zombies[i]->pid)) {
            return i;
        }
    }

    return -1;
}

void reap_process(sched::process *zombie) {
    sched::sched_lock.irq_acquire();

    auto task = zombie->main_thread;
    while (task->state == sched::thread::BLOCKED) { asm volatile("pause"); }
    task->state = sched::thread::DEAD;
    if (task->cpu != -1) {
        auto cpu = task->cpu;

        sched::sched_lock.irq_release();
        apic::lapic::ipi(cpu, 253);
        while (task->cpu != -1) { asm volatile("pause"); };
        sched::sched_lock.irq_acquire();
    }

    sched::threads[task->tid] = nullptr;
    sched::processes[zombie->pid] = nullptr;

    frg::destruct(memory::mm::heap, task);
    frg::destruct(memory::mm::heap, zombie->mail);
    frg::destruct(memory::mm::heap, zombie->sig_queue.mail);
    frg::destruct(memory::mm::heap, zombie);    

    sched::sched_lock.irq_release();
}

frg::tuple<uint32_t, sched::pid_t> sched::process::waitpid(sched::pid_t pid, thread *waiter, int options) {
    // Reap zombies first
    this->lock.irq_acquire();
    for (size_t i = 0; i < zombies.size(); i++) {
        process *zombie = zombies[i];
        if (zombie == nullptr) continue;
        if (pid < -1) {
            if (zombie->group->pgid != zombie->pid) {
                continue;
            }
        } else if (pid == 0) {
            if (zombie->group->pgid != group->pgid) {
                continue;
            }
        } else if (pid > 0) {
            if (zombie->pid != pid) {
                continue;
            }
        }

        zombies[i] = nullptr;

        uint8_t status = zombie->status.val;
        pid_t pid = zombie->pid;
        reap_process(zombie);

        this->lock.irq_release();
        return {status, pid};
    }

    if (options & WNOHANG) {
        this->lock.irq_release();
        return {0, 0};
    }

    process *proc = nullptr;
    pid_t return_pid = 0;
    uint32_t exit_val = 0;

    do_wait:
        while (true) {
            auto msg = mail->recv(true, -1, waiter);
            if (msg == nullptr) {
                goto finish;
            }

            if (pid < -1 && msg->who.proc->group->pgid != pid) {
                continue;
            } else if (pid == 0 && msg->who.proc->group->pgid != group->pgid) {
                continue;
            } else if (pid > 0 && msg->who.pid != pid){
                continue;
            }

            proc = msg->who.proc;
            frg::destruct(memory::mm::heap, msg);
            break;
        }

        if (!(options & WUNTRACED) && proc->status.val == STOPPED) goto do_wait;
        if (!(options & WUNTRACED) && proc->status.val == RUNNING) goto do_wait;

        return_pid = proc->pid;
        exit_val = proc->status.val;

        if ((exit_val & TERMINATED) == TERMINATED) {
            zombies[find_zombie(proc)] = nullptr;
            reap_process(proc);
        }
    finish:
        this->lock.irq_release();
        return {exit_val, return_pid};
}


int64_t sched::pick_task() {
    for (int64_t t = smp::get_tid() + 1; (uint64_t) t < threads.size(); t++) {
        auto task = threads[t];
        if (task) {
            if (task->state == thread::READY || task->state == thread::WAIT) {
                return task->tid;
            }
        }
    }

    for (int64_t t = 0; t < smp::get_tid() + 1; t++) {
        auto task = threads[t];
        if (task) {
            if (task->state == thread::READY || task->state == thread::WAIT) {
                return task->tid;
            }
        }
    }

    return -1;
}

void sched::swap_task(irq::regs *r) {
    sched_lock.acquire();

    auto running_task = smp::get_thread();
    if (running_task) {
        running_task->reg.rax = r->rax;
        running_task->reg.rbx = r->rbx;
        running_task->reg.rcx = r->rcx;
        running_task->reg.rdx = r->rdx;
        running_task->reg.rbp = r->rbp;
        running_task->reg.rdi = r->rdi;
        running_task->reg.rsi = r->rsi;
        running_task->reg.r8 = r->r8;
        running_task->reg.r9 = r->r9;
        running_task->reg.r10 = r->r10;
        running_task->reg.r11 = r->r11;
        running_task->reg.r12 = r->r12;
        running_task->reg.r13 = r->r13;
        running_task->reg.r14 = r->r14;
        running_task->reg.r15 = r->r15;

        running_task->reg.rflags = r->rflags;
        running_task->reg.rip = r->rip;
        running_task->reg.rsp = r->rsp;

        running_task->reg.cs = r->cs;
        running_task->reg.ss = r->ss;

        running_task->kstack = smp::get_locals()->tss.rsp0;
        running_task->ustack = smp::get_locals()->ustack;

        running_task->stopped = io::tsc();
        running_task->uptime += running_task->stopped - running_task->started;
        running_task->cpu = -1;

        running_task->reg.cr3 = memory::vmm::read_cr3();

        if (running_task->running) {
            running_task->running = false;
        }

        if (running_task->state == thread::RUNNING && running_task->tid != smp::get_locals()->idle_tid) {
            running_task->state = thread::READY;
        }
    }

    int64_t next_tid = pick_task();
    if (next_tid == -1) {
        if (running_task->tid != smp::get_locals()->idle_tid && running_task->pid != -1) {
            signal::process_signals(running_task->proc, &running_task->reg);
            goto swap_regs;
        }

        smp::get_locals()->task = threads[smp::get_locals()->idle_tid];
        smp::get_locals()->tid =  smp::get_locals()->idle_tid;
        running_task = smp::get_thread();
    } else {
        auto next_task = threads[next_tid];
        if (next_task->pid != -1) {
            signal::process_signals(next_task->proc, &next_task->reg);
        }

        smp::get_locals()->proc = next_task->proc;
        smp::get_locals()->task = next_task;
        smp::get_locals()->tid = next_tid;
        smp::get_locals()->pid = next_task->pid;

        running_task = next_task;
        running_task->running = true;
        running_task->state = thread::RUNNING;
    }

    swap_regs:

    running_task->cpu = smp::get_locals()->lid;
    r->rax = running_task->reg.rax;
    r->rbx = running_task->reg.rbx;
    r->rcx = running_task->reg.rcx;
    r->rdx = running_task->reg.rdx;
    r->rbp = running_task->reg.rbp;
    r->rdi = running_task->reg.rdi;
    r->rsi = running_task->reg.rsi;
    r->r8 = running_task->reg.r8;
    r->r9 = running_task->reg.r9;
    r->r10 = running_task->reg.r10;
    r->r11 = running_task->reg.r11;
    r->r12 = running_task->reg.r12;
    r->r13 = running_task->reg.r13;
    r->r14 = running_task->reg.r14;
    r->r15 = running_task->reg.r15;

    r->rflags = running_task->reg.rflags;
    r->rip = running_task->reg.rip;
    r->rsp = running_task->reg.rsp;

    r->cs = running_task->reg.cs;
    r->ss = running_task->reg.ss;

    if (running_task->proc) {
        io::wrmsr(smp::fsBase, running_task->proc->user_fs);
    }

    smp::get_locals()->kstack = running_task->kstack;
    smp::get_locals()->tss.rsp0 = running_task->kstack;
    smp::get_locals()->tss.ist[0] = running_task->kstack;
    smp::get_locals()->ustack = running_task->ustack;

    running_task->started = io::tsc();

    if (memory::vmm::read_cr3() != running_task->reg.cr3) {
        memory::vmm::write_cr3(running_task->reg.cr3);
    }

    sched_lock.release();
}