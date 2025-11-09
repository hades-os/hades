#ifndef MAIL_HPP
#define MAIL_HPP

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
    struct message {
        struct {
            sched::thread *task;
            sched::process *proc;
            
            sched::pid_t pid;
            sched::tid_t tid;
        } who;

        int what;
        void *data;
    };

    struct port;
    struct mailbox {
        private:
            struct message_node {
                message *msg;
                message_node *next;
            };

            message_node *head;
            message_node *tail;

            frg::vector<message *, memory::mm::heap_allocator> messages;

            message *latest_message;

            size_t size;
            util::lock lock;

            sched::process *owner;
            sched::thread *current_waiter;
            port *time_port;
        public:
            sched::timespec time;

            message *recv(bool block, int filter_what, sched::thread* receiver);
            void set_timer(sched::timespec *time);

            port *make_port();
            port *timer_port();
            
            mailbox(sched::process *owner): head(nullptr), tail(nullptr), messages(), size(0), lock(), 
                                            owner(owner), current_waiter(nullptr), time_port(make_port()) {};
            friend struct port;
    };

    struct port {
        private:
            mailbox *mail;
        public:

            friend struct mailbox;
            void post(message msg);
    };
}

#endif