
#include "sys/pit.hpp"
#include <mm/mm.hpp>
#include <sys/sched/mail.hpp>
#include <sys/sched/sched.hpp>

ipc::message *ipc::mailbox::recv(bool block, int filter_what, sched::thread* receiver) {
    if (!block && __atomic_load_n(&this->size, __ATOMIC_SEQ_CST) == 0) {
        return nullptr;
    }

    owner->sig_queue.active = true;
    // if we have messages, check them first
    if (__atomic_load_n(&this->size, __ATOMIC_SEQ_CST) > 0) {
        lock.irq_acquire();
        if (filter_what > 0) {
            for (auto msg: messages) {
                if (filter_what == msg->what) {
                    auto msg = this->messages.pop();
                    __atomic_fetch_sub(&this->size, 1, __ATOMIC_SEQ_CST);
                    lock.irq_release();

                    return msg;
                }
            }

            lock.irq_release();
            return nullptr;
        } else {
            auto msg = this->messages.pop();
            __atomic_fetch_sub(&this->size, 1, __ATOMIC_SEQ_CST);
            lock.irq_release();

            return msg;
        }
    }

    wait_for_message:
        lock.irq_acquire();
        current_waiter = receiver;
        lock.irq_release();

        receiver->state = sched::thread::BLOCKED;
        while (latest_message == nullptr || receiver->state == sched::thread::BLOCKED) sched::retick();

    lock.irq_acquire();
    current_waiter = nullptr;

    owner->sig_queue.active = false;
    // We were interrupted by a signal, drop out
    if (owner->block_signals) {
        if (latest_message) {
            messages.push_back(latest_message);
            __atomic_fetch_add(&this->size, 1, __ATOMIC_SEQ_CST);
            latest_message = nullptr;
            lock.irq_release();
        }

        owner->block_signals = false;
        return nullptr;
    }

    // if we have a filter, do we match it?
    if (filter_what > 0) {
        if (latest_message->what == filter_what) {
            auto msg = latest_message;
            latest_message = nullptr;
            lock.irq_release();

            return msg;
        }  else {
            // message, but no bueno
            messages.push_back(latest_message);
            __atomic_fetch_add(&this->size, 1, __ATOMIC_SEQ_CST);
            latest_message = nullptr;
            lock.irq_release();

            goto wait_for_message;
        }
    }

    // we have a matching message, return it without appending
    auto msg = latest_message;
    latest_message = nullptr;
    lock.irq_release();

    return msg;
}

void ipc::mailbox::set_timer(sched::timespec *time) {
    lock.irq_acquire();
    pit::timers_lock.irq_acquire();

    this->time = *time;
    pit::timers.push_back(this);

    pit::timers_lock.irq_release();
    lock.irq_release();
}

ipc::port *ipc::mailbox::make_port() {
    auto port = frg::construct<ipc::port>(memory::mm::heap);
    port->mail = this;

    return port;
}

ipc::port *ipc::mailbox::timer_port() {
    return time_port;
}

void ipc::port::post(message msg) {
    message *out_msg = frg::construct<message>(memory::mm::heap);
    memcpy(out_msg, &msg, sizeof(message));

    mail->lock.irq_acquire();
    mail->latest_message = out_msg;
    if (mail->current_waiter) mail->current_waiter->state = sched::thread::READY;
    mail->lock.irq_release();
}

/*
frg::tuple<bool, sched::wait::trigger*> sched::wait::queue::block(thread *task) {
    lock.irq_acquire();
    tasks.push(task);
    lock.irq_release();

    task->proc->sig_queue.active = true;
    task->state = thread::BLOCKED;

    while (task->state == thread::BLOCKED) retick();
    task->proc->sig_queue.active = false;

    if (task->proc->block_signals) {
        task->proc->block_signals = false;
        // TODO: set errno
        return {false, nullptr};
    }

    return {true, task->last_trigger};
} */