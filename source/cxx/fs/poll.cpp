
#include <cstddef>
#include <frg/hash_map.hpp>
#include <arch/types.hpp>
#include <mm/mm.hpp>
#include <sys/sched/time.hpp>
#include <sys/sched/sched.hpp>
#include <fs/poll.hpp>
#include <util/lock.hpp>
#include "ipc/evtable.hpp"
#include "mm/slab.hpp"
#include "util/types.hpp"

void poll::producer::arise(ssize_t event) {
    util::lock_guard guard{lock};

    for (auto table: tables) {
        table->arise(event, self);
    }
}

poll::producer::~producer() {
    for (auto table: tables) {
        table->disconnect(self);
    }
}

void poll::table::arise(ssize_t event, shared_ptr<producer> waker) {
    util::lock_guard guard{lock};

    latest_producer = waker;
    latest_event = event;

    events.push_back({
        .producer = waker,
        .events = event
    });

    wire.arise(evtable::NEW_MESSAGE);
}

frg::tuple<
    shared_ptr<poll::producer>, 
    ssize_t
> poll::table::wait(bool allow_signals, sched::timespec *timeout) {
    if (latest_producer) {
        util::lock_guard guard{lock};

        auto return_producer = latest_producer;
        auto return_event = latest_event;

        latest_producer = shared_ptr<poll::producer>{};
        latest_event = -1;

        events.pop();
        return {return_producer, return_event};
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

    auto return_producer = latest_producer;
    auto return_event = latest_event;

    latest_producer = shared_ptr<poll::producer>{};
    latest_event = -1;

    events.pop();
    return {return_producer, return_event};
} 

void poll::table::connect(shared_ptr<producer> producer) {
    util::lock_guard guard{lock};
    util::lock_guard producer_guard{producer->lock};
    
    producers.push(producer);
    producer->tables.push(self);
}

void poll::table::disconnect(shared_ptr<producer> producer) {
    util::lock_guard guard{lock};
    util::lock_guard producer_guard{producer->lock};

    producers.erase(producer);
    producer->tables.erase(self);
}

poll::table::~table() {
    for (auto producer: producers) {
        disconnect(producer);
    }
}

shared_ptr<poll::table> poll::create_table() {
    auto table = prs::allocate_shared<poll::table>(mm::slab<poll::table>());
    table->self = table;

    return table;
}

shared_ptr<poll::producer> poll::create_producer(weak_ptr<vfs::descriptor> desc) {
    auto producer = prs::allocate_shared<poll::producer>(mm::slab<poll::producer>(), desc);
    producer->self = producer;

    return producer;
}