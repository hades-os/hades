#ifndef NAMESPACE_HPP
#define NAMESPACE_HPP

#include <cstddef>
#include "frg/rcu_radixtree.hpp"
#include "fs/vfs.hpp"
#include "mm/arena.hpp"
#include "prs/allocator.hpp"
#include "prs/vector.hpp"
#include "util/lock.hpp"
#include "util/types.hpp"

namespace sched {
    struct process;
    struct process_group;
    struct session;
}

namespace ns {
    struct pid {
        private:
            util::spinlock process_lock;
            util::spinlock process_group_lock;
            util::spinlock session_lock;

            prs::allocator allocator;

            weak_ptr<pid> self;
            weak_ptr<pid> parent;
            prs::vector<shared_ptr<pid>, prs::allocator>
                children;

            weak_ptr<sched::thread> init;
            weak_ptr<sched::thread> true_init;

            frg::rcu_radixtree<sched::process *, prs::allocator>
                process_tree;
            frg::rcu_radixtree<sched::process_group *, prs::allocator>
                process_group_tree;
            frg::rcu_radixtree<sched::session *, prs::allocator>
                session_tree;
        public:
            pid(weak_ptr<pid> parent,
                weak_ptr<sched::thread> init, weak_ptr<sched::thread> true_init):
                process_lock(), process_group_lock(), session_lock(), 
                allocator(arena::create_resource()),
                parent(parent), children(allocator),
                init(init), true_init(true_init),
                process_tree(allocator), process_group_tree(allocator),
                session_tree(allocator) {}

            sched::process *create_process(char *name, void (*main)(), 
                uint64_t rsp, vmm::vmm_ctx *ctx, uint8_t privilege);
            sched::process_group *create_process_group(sched::process *leader);
            sched::session *create_session(sched::process *leader, sched::process_group *group);

            pid_t add_process(sched::process *proc);
            pid_t add_process_group(sched::process_group *group);
            pid_t add_session(sched::session *sess);

            void remove_process(pid_t pid);
            void remove_process_group(pid_t pgid);
            void remove_session(pid_t sid);

            sched::process *get_process(pid_t pid);
            sched::process_group *get_process_group(pid_t pgid);
            sched::session *get_session(pid_t sid);               
    };

    static size_t zero = 0;
    struct mount {
        private:
            weak_ptr<mount> self;

            shared_ptr<vfs::node> resolve_root;
            weak_ptr<vfs::node> true_root;

            weak_ptr<mount> parent;
            prs::vector<shared_ptr<mount>, prs::allocator>
                children;
        public:
            mount(weak_ptr<ns::mount> self,
                shared_ptr<vfs::node> resolve_root, weak_ptr<vfs::node> true_root,
                weak_ptr<mount> parent):
                self(self),
                resolve_root(resolve_root), true_root(true_root),
                parent(parent), children(arena::create_resource()) {}

            weak_ptr<vfs::filesystem> resolve_fs(prs::string_view path, 
                shared_ptr<vfs::node> base, size_t& symlinks_traversed = zero);
            shared_ptr<vfs::node> resolve_at(prs::string_view path, 
                shared_ptr<vfs::node> base, bool follow_symlink = true, size_t& symlinks_traversed = zero);
            weak_ptr<vfs::node> resolve_parent(shared_ptr<vfs::node> base, prs::string_view path);
            vfs::path resolve_abspath(shared_ptr<vfs::node> node);

            bool child_of(weak_ptr<ns::mount> other);
            bool has_root();
            void set_root(shared_ptr<vfs::node> new_root);
    };

    struct accessor {
        shared_ptr<pid> pid_ns;
        shared_ptr<mount> mount_ns;

        accessor(shared_ptr<pid> pid_ns,
            shared_ptr<mount> mount_ns):
            pid_ns(pid_ns), mount_ns(mount_ns) {}
    };
}

#endif