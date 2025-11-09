
#include "sys/sched/time.hpp"
#include "sys/smp.hpp"
#include <cstddef>
#include <mm/mm.hpp>
#include <sys/sched/wait.hpp>
#include <sys/sched/sched.hpp>
#include <sys/pit.hpp>

sched::thread *ipc::queue::block(sched::thread *waiter, bool *set_when_blocking) {
    lock.irq_acquire();
    waiters.push_back(waiter);
    lock.irq_release();

    if (waiter->proc) {
        waiter->proc->sig_queue.active = true;
    }

    if (set_when_blocking) *set_when_blocking = true;
    waiter->state = sched::thread::BLOCKED;
    while (waiter->state == sched::thread::BLOCKED) sched::retick();

    if (waiter->proc) {
        waiter->proc->sig_queue.active = false;
    }

    if (waiter->proc && waiter->proc->block_signals) {
        waiter->proc->block_signals = false;
        smp::set_errno(EINTR);
        return nullptr;
    }

    return last_waker;
}

void ipc::queue::set_timer(sched::timespec *time) {
    this->time = *time;
    this->timer_trigger = frg::construct<trigger>(memory::mm::heap);
    timer_trigger->add(this);

    sched::timer *timer = frg::construct<sched::timer>(memory::mm::heap);
    timer->spec = *time;

    timer->triggers.push_back(timer_trigger);
    pit::timers.push_back(timer);

    // TODO: timer
}

void ipc::trigger::add(queue *waitq) {
    lock.irq_acquire();
    queues.push_back(waitq);
    lock.irq_release();
}

void ipc::trigger::remove(queue *waitq) {
    lock.irq_acquire();
    for (size_t i = 0; i < queues.size(); i++) {
        auto queue = queues[i];
        if (queue == nullptr) continue;
        if (queue == waitq) {
            queues[i] = nullptr;
            return;
        }
    }
    lock.irq_release();
}

void ipc::trigger::arise(sched::thread *waker) {
    lock.irq_acquire();

    for (size_t i = 0; i < queues.size(); i++) {
        auto waitq = queues[i];
        if (waitq == nullptr) continue;

        waitq->lock.irq_acquire();
        waitq->last_waker = waker;
        for (size_t j = 0; j < waitq->waiters.size(); j++) {
            auto waiter = waitq->waiters[j];
            if (waiter == nullptr) continue;
            
            waiter->state = sched::thread::READY;
        }
        
        waitq->waiters.clear();
        waitq->lock.irq_release();
    }

    lock.irq_release();
}

void ipc::trigger::clear() {
    lock.irq_acquire();
    queues.clear();
    lock.irq_release();
}
