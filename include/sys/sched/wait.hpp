#ifndef WAIT_HPP
#define WAIT_HPP

#include <cstddef>
#include <frg/tuple.hpp>
#include <util/lock.hpp>
#include <cstdint>
#include <frg/vector.hpp>
#include <mm/mm.hpp>
#include <sys/sched/time.hpp>

namespace sched {
    class thread;
    class process;

    using pid_t = int64_t;
    using tid_t = int64_t;
};

namespace ipc {
    struct trigger;
    struct queue;
    
    struct trigger {
        private:
            frg::vector<queue *, memory::mm::heap_allocator> queues;
            util::lock lock;
        public:
            void add(queue *waitq);
            void remove(queue *waitq);

            void arise(sched::thread *waker);
            void clear();

            trigger(): queues(), lock() {}
    };

    struct queue {
        private:
            sched::thread *last_waker;
            
            frg::vector<sched::thread *, memory::mm::heap_allocator> waiters;
            util::lock lock;

            sched::timespec time;
            trigger *timer_trigger;
        public:
            friend struct trigger;

            void set_timer(sched::timespec *time);
            sched::thread *block(sched::thread *waiter, bool *set_when_blocking = nullptr);

            queue(): last_waker(nullptr), waiters(), lock(), timer_trigger(nullptr) {}
    };
}

#endif