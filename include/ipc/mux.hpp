#ifndef MUX_HPP
#define MUX_HPP

#include "arch/types.hpp"
#include "frg/hash_map.hpp"
#include "mm/mm.hpp"
#include "util/types.hpp"
#include <cstddef>
#include <util/lock.hpp>
#include <frg/vector.hpp>
#include <frg/tuple.hpp>
#include <sys/sched/time.hpp>

namespace sched {
    class thread;
};

namespace mux {
    struct source;
    struct drain;

    shared_ptr<source> create_source();
    shared_ptr<drain> create_drain();

    struct source {
        private:
            friend struct drain;
            friend shared_ptr<source> mux::create_source();

            shared_ptr<source> selfPtr;

            frg::vector<shared_ptr<drain>, memory::mm::heap_allocator> drains;
            util::spinlock lock;
        public:
            source(): selfPtr(), drains(), lock() {}

            void drain_to(shared_ptr<drain> drain);
            void arise(ssize_t event);
    };

    struct drain {
        private:
            friend struct source;
            friend void arch::tick_clock(long);
            friend shared_ptr<drain> create_drain();

            shared_ptr<drain> selfPtr;

            frg::vector<shared_ptr<source>, memory::mm::heap_allocator> sources;
            frg::vector<sched::thread *, memory::mm::heap_allocator> threads;

            ssize_t latest_event;
            sched::thread *latest_waker;
            
            util::spinlock lock;

            void wait_for_wake(sched::thread *);
            void arise(ssize_t event, sched::thread *waker);
        public:
            drain(): selfPtr(), threads(), lock() {}

            drain(drain&& other): threads(std::move(other.threads)),
                    latest_event(std::move(other.latest_event)), latest_waker(std::move(other.latest_waker)),
                    lock() {}
            
            frg::tuple<ssize_t, sched::thread *> wait(bool allow_signals = false, sched::timespec *timeout = nullptr);

            void disconnect(shared_ptr<source> source);
    };
}

#endif