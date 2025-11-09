
#include "smarter/smarter.hpp"
#include "util/types.hpp"
#include <cstddef>
#include <frg/hash_map.hpp>
#include <arch/types.hpp>
#include <mm/mm.hpp>
#include <sys/sched/time.hpp>
#include <sys/sched/sched.hpp>
#include <ipc/mux.hpp>
#include <util/lock.hpp>

void mux::drain::wait_for_wake(sched::thread *thread) {
    util::lock_guard guard{lock};
    threads.append(thread);
    guard.~lock_guard();

    arch::stop_thread(thread);

    arch::irq_on();
    while (thread->state == sched::thread::BLOCKED && !thread->pending_signal) arch::tick();

    util::lock_guard reguard{lock};
    threads.erase(thread);
    reguard.~lock_guard();
}

frg::tuple<ssize_t, sched::thread *>
    mux::drain::wait(bool allow_signals, sched::timespec *timeout) {
    auto thread = arch::get_thread();

    if (timeout) {
        arch::add_timer({
            .spec = *timeout,
            .drain = this
        });
    }

    bool irqs_enabled = arch::get_irq_state();
    wait_for_event:
        wait_for_wake(thread);

    if (!allow_signals && thread->pending_signal) {
        goto wait_for_event;
    }

    if (irqs_enabled) {
        arch::irq_on();
    } else {
        arch::irq_off();
    }

    if (thread->pending_signal) {
        arch::set_errno(EINTR);
        return {-1, nullptr};
    }

    return {latest_event, latest_waker};
}

void mux::drain::arise(ssize_t event, sched::thread *waker) {
    util::lock_guard guard{lock};

    latest_waker = waker;
    latest_event = event;

    for (auto thread: threads) {
        arch::start_thread(thread);
    }
}

void mux::drain::disconnect(shared_ptr<source> source) {
    util::lock_guard guard{lock};
    util::lock_guard source_guard{source->lock};

    sources.erase(source);
    source->drains.erase(selfPtr);
}

void mux::source::drain_to(shared_ptr<drain> drain) {
    util::lock_guard guard{lock};

    drains.append(drain);
    drain->sources.append(selfPtr);
}

void mux::source::arise(ssize_t event) {
    util::lock_guard guard{lock};

    for (auto drain: drains) {
        if (drain)
            drain->arise(event, arch::get_thread());
    }
}

shared_ptr<mux::source> mux::create_source() {
    auto source = smarter::allocate_shared<mux::source>(memory::mm::heap);
    source->selfPtr = source;

    return source;
}

shared_ptr<mux::drain> mux::create_drain() {
    auto drain = smarter::allocate_shared<mux::drain>(memory::mm::heap);
    drain->selfPtr = drain;

    return drain;
}