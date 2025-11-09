
#include <cstddef>
#include <frg/hash_map.hpp>
#include <arch/types.hpp>
#include <mm/mm.hpp>
#include <sys/sched/time.hpp>
#include <sys/sched/sched.hpp>
#include <fs/poll.hpp>
#include <util/lock.hpp>
#include "ipc/evtable.hpp"
#include "util/types.hpp"

void poll::queue::arise(ssize_t event) {
    util::lock_guard guard{lock};

    for (auto table: tables) {
        table->arise(event, selfPtr);
    }
}

poll::queue::~queue() {
    for (auto table: tables) {
        table->disconnect(selfPtr);
    }
}

void poll::table::arise(ssize_t event, shared_ptr<queue> waker) {
    util::lock_guard guard{lock};

    latest_queue = waker;
    latest_event = event;

    events.push_back(event);
    wire.arise(evtable::NEW_MESSAGE);
}

frg::tuple<
    shared_ptr<poll::queue>, 
    ssize_t
> poll::table::wait(bool allow_signals, sched::timespec *timeout) {
    if (latest_queue) {
        util::lock_guard guard{lock};

        auto return_queue = latest_queue;
        auto return_event = latest_event;

        latest_queue = shared_ptr<poll::queue>{};
        latest_event = -1;

        events.pop();
        return {return_queue, return_event};
    }

    wait_for_event:
        auto [evt, _] = wire.wait(evtable::NEW_MESSAGE, allow_signals, timeout);
    
        if (evt < 0) {
        if (allow_signals)
            return {{}, -1};
        else
            goto wait_for_event;
    }

    util::lock_guard guard{lock};

    auto return_queue = latest_queue;
    auto return_event = latest_event;

    latest_queue = shared_ptr<poll::queue>{};
    latest_event = -1;

    events.pop();
    return {return_queue, return_event};
} 

void poll::table::connect(shared_ptr<queue> queue) {
    util::lock_guard guard{lock};
    util::lock_guard queue_guard{queue->lock};

    queues.push(queue);
    queue->tables.push(selfPtr);
}

void poll::table::disconnect(shared_ptr<queue> queue) {
    util::lock_guard guard{lock};
    util::lock_guard queue_guard{queue->lock};

    queues.erase(queue);
    queue->tables.erase(selfPtr);
}

poll::table::~table() {
    for (auto queue: queues) {
        disconnect(queue);
    }
}

shared_ptr<poll::table> poll::create_table() {
    auto table = smarter::allocate_shared<poll::table>(memory::mm::heap);
    table->selfPtr = table;

    return table;
}

shared_ptr<poll::queue> poll::create_queue() {
    auto queue = smarter::allocate_shared<poll::queue>(memory::mm::heap);
    queue->selfPtr = queue;

    return queue;
}