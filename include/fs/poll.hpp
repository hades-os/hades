#ifndef POLL_HPP
#define POLL_HPP

#include "ipc/wire.hpp"
#include "mm/mm.hpp"
#include "util/types.hpp"
#include <util/lock.hpp>
#include <frg/vector.hpp>
#include <frg/tuple.hpp>

namespace sched {
    class thread;
};

namespace poll {
    struct queue;
    struct table;

    shared_ptr<queue> create_queue();
    shared_ptr<table> create_table();

    struct queue {
        private:
            friend struct table;
            friend shared_ptr<queue> create_queue();

            shared_ptr<queue> self;

            frg::vector<shared_ptr<table>, memory::mm::heap_allocator> tables;
            util::spinlock lock;
        public:
            queue(): tables(), lock() {}
            ~queue();

            queue(queue&& other): tables(other.tables), lock() {}

            void arise(ssize_t event);      
    };

    struct table {
        private:
            friend struct queue;
            friend shared_ptr<table> create_table();
 
            shared_ptr<table> self;

            frg::vector<ssize_t, memory::mm::heap_allocator> events;

            frg::vector<shared_ptr<queue>, memory::mm::heap_allocator> queues;
            shared_ptr<queue> latest_queue;
            ssize_t latest_event;

            ipc::wire wire;
            
            util::spinlock lock;

            void arise(ssize_t event, shared_ptr<queue> waker);
        public:
            table(): self(), events(), queues(), wire(), lock() {}
            ~table();

            frg::tuple<shared_ptr<queue>, ssize_t> wait(bool allow_signals, sched::timespec *timeout);
            frg::vector<ssize_t, memory::mm::heap_allocator> &get_events();

            void connect(shared_ptr<queue> queue);
            void disconnect(shared_ptr<queue> queue);      
    };
}

#endif