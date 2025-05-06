#ifndef POLL_HPP
#define POLL_HPP

#include "ipc/wire.hpp"
#include "mm/arena.hpp"
#include "mm/mm.hpp"
#include "util/types.hpp"
#include <util/lock.hpp>
#include <frg/vector.hpp>
#include <frg/tuple.hpp>

namespace sched {
    class thread;
};

namespace vfs {
    struct descriptor;
}

namespace poll {
    struct producer;
    struct table;

    shared_ptr<producer> create_producer(weak_ptr<vfs::descriptor> desc);
    shared_ptr<table> create_table();

    union producer_context {
        void * p;
        int number;
        uint32_t  number_32;
        uint64_t  number_64;
    };

    struct producer {
        private:
            friend struct table;
            friend shared_ptr<producer> create_producer(weak_ptr<vfs::descriptor> desc);

            shared_ptr<poll::producer> self;
            weak_ptr<vfs::descriptor> desc;
            producer_context ctx;

            frg::vector<shared_ptr<table>, prs::allocator> tables;
            util::spinlock lock;
        public:
            producer(weak_ptr<vfs::descriptor> desc): desc(desc), tables(arena::create_resource()), lock() {}
            ~producer();

            producer(producer&& other): tables(std::move(other.tables)), lock() {}

            void arise(ssize_t event);      
    };

    struct table {
        private:
            friend struct producer;
            friend shared_ptr<table> create_table();
 
            shared_ptr<table> self;

            struct event {
                shared_ptr<poll::producer> producer;
                ssize_t events;
            };

            arena::arena_resource *resource;

            frg::vector<event, prs::allocator> events;
            frg::vector<shared_ptr<producer>, prs::allocator> producers;

            shared_ptr<producer> latest_producer;
            ssize_t latest_event;

            ipc::wire wire;
            
            util::spinlock lock;

            void arise(ssize_t event, shared_ptr<producer> waker);
        public:
            table(): self(), resource(arena::create_resource()),
                events(resource), producers(resource), wire(), lock() {}
            ~table();

            frg::tuple<shared_ptr<producer>, ssize_t> wait(bool allow_signals, sched::timespec *timeout);
            frg::vector<event, prs::allocator> &get_events() {
                return events;
            }

            void connect(shared_ptr<producer> producer);
            void disconnect(shared_ptr<producer> producer);      
    };
}

#endif