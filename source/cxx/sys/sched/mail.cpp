
#include "sys/pit.hpp"
#include <cstddef>
#include <mm/mm.hpp>
#include <sys/sched/mail.hpp>
#include <sys/sched/sched.hpp>

ipc::message *ipc::mailbox::recv(bool block, int filter_what, sched::thread* receiver) {
    if (!block && __atomic_load_n(&this->size, __ATOMIC_SEQ_CST) == 0) {
        return nullptr;
    }

    if (owner)
        owner->sig_queue.active = true;
    // if we have messages, check them first
    lock.irq_acquire();
    if (__atomic_load_n(&this->size, __ATOMIC_SEQ_CST) > 0) {
        if (filter_what > 0) {
            for (size_t i = 0; i < messages.size(); i++) {
                auto msg = this->messages[i];
                if (msg == nullptr) continue;
                if (filter_what == msg->what) {
                    __atomic_fetch_sub(&this->size, 1, __ATOMIC_SEQ_CST);
                    this->messages[i] = nullptr;
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
    lock.irq_release();

    wait_for_message:
        lock.irq_acquire();
        current_waiter = receiver;
        receiver->state = sched::thread::BLOCKED;
        lock.irq_release();

        while (receiver->state == sched::thread::BLOCKED) sched::retick();

    lock.irq_acquire();
    current_waiter = nullptr;

    if (owner)
        owner->sig_queue.active = false;
    // We were interrupted by a signal, drop out
    if (owner && owner->block_signals) {
        if (latest_message) {
            messages.push_back(latest_message);
            __atomic_fetch_add(&this->size, 1, __ATOMIC_SEQ_CST);
            latest_message = nullptr;
        }

        owner->block_signals = false;
        lock.irq_release();

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
            current_waiter = receiver;
            lock.irq_release();

            goto wait_for_message;
        }
    }

    // we have a message, return it without appending
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