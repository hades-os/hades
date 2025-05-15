#ifndef NAMESPACE_HPP
#define NAMESPACE_HPP

#include <cstddef>
#include "frg/rcu_radixtree.hpp"
#include "fs/vfs.hpp"
#include "mm/arena.hpp"
#include "prs/allocator.hpp"
#include "prs/vector.hpp"
#include "sys/sched/sched.hpp"
#include "util/lock.hpp"
#include "util/types.hpp"

namespace ns {
    struct pid {
        private:
            util::spinlock lock;
            prs::allocator allocator;

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
                lock(), allocator(arena::create_resource()),
                parent(parent), children(allocator),
                init(init), true_init(true_init),
                process_tree(allocator), process_group_tree(allocator),
                session_tree(allocator) {}
    };

    static size_t zero = 0;
    struct mount {
        private:
            shared_ptr<vfs::node> resolve_root;
            weak_ptr<vfs::node> true_root;
        public:
            weak_ptr<vfs::filesystem> resolve_fs(frg::string_view path, 
                shared_ptr<vfs::node> base, size_t& symlinks_traversed = zero);
            shared_ptr<vfs::node> resolve_at(frg::string_view path, 
                shared_ptr<vfs::node> base, bool follow_symlink = true, size_t& symlinks_traversed = zero);
            weak_ptr<vfs::node> resolve_parent(shared_ptr<vfs::node> base, frg::string_view path);
            vfs::path resolve_abspath(shared_ptr<vfs::node> node);

            shared_ptr<vfs::node> make_recursive(frg::string_view path, 
                shared_ptr<vfs::node> base, int64_t type, mode_t mode);
    };

    struct accessor {
        unique_ptr<pid> pid_ns;
        unique_ptr<mount> mount_ns;
    };
}

#endif