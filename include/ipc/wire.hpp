#ifndef EVENT_HPP
#define EVENT_HPP

#include "frg/hash_map.hpp"
#include "mm/mm.hpp"
#include "mm/slab.hpp"
#include "prs/allocator.hpp"
#include "prs/vector.hpp"
#include <cstddef>
#include <util/lock.hpp>
#include <frg/tuple.hpp>
#include <sys/sched/time.hpp>

namespace sched {
    struct thread;
};

namespace ipc {
    struct  wire {
        private:
            prs::vector<sched::thread *, prs::allocator> threads;

            ssize_t latest_event;
            sched::thread *latest_waker;
            
            util::spinlock lock;

            void wait_for_wake(sched::thread *);
        public:
            wire(): threads(slab::create_resource()), lock() {}

            wire(wire&& other): threads(std::move(other.threads)),
                latest_event(std::move(other.latest_event)), latest_waker(std::move(other.latest_waker)),
                lock() {}

            frg::tuple<ssize_t, sched::thread *> wait(ssize_t event, bool allow_signals = false, sched::timespec *timeout = nullptr);
            void arise(ssize_t event);
    };
}

#endif