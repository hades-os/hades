#include <arch/types.hpp>
#include <frg/rcu_radixtree.hpp>
#include <sys/sched/sched.hpp>
#include <util/lock.hpp>
#include <util/types.hpp>
#include "mm/arena.hpp"
#include "sys/namespace.hpp"

pid_t ns::pid::add_process(sched::process *proc) {
    util::lock_guard guard{process_lock};
    
    pid_t pid = arch::allocate_pid();
    process_tree.insert(pid, proc);
    return pid;
}

pid_t ns::pid::add_process_group(sched::process_group *group) {
    util::lock_guard guard{process_group_lock};

    pid_t pgid = group->pgid;
    process_group_tree.insert(pgid, group);
    return pgid;
}

pid_t ns::pid::add_session(sched::session *sess) {
    util::lock_guard guard{session_lock};

    pid_t sid = sess->sid;
    session_tree.insert(sid, sess);
    return sid;
}

void ns::pid::remove_process(pid_t pid) {
    util::lock_guard guard{process_lock};

    process_tree.erase(pid);
}

void ns::pid::remove_process_group(pid_t pgid) {
    util::lock_guard guard{process_group_lock};

    process_group_tree.erase(pgid);
}

void ns::pid::remove_session(pid_t sid) {
    util::lock_guard guard{session_lock};

    session_tree.erase(sid);
}

sched::process *ns::pid::get_process(pid_t pid) {
    util::lock_guard guard{process_lock};

    auto process_ptr = process_tree.find(pid);
    if (process_ptr == nullptr) return nullptr;
    return *process_ptr;
}

sched::process_group *ns::pid::get_process_group(pid_t pgid) {
    util::lock_guard guard{process_group_lock};

    auto process_group_ptr = process_group_tree.find(pgid);
    if (process_group_ptr == nullptr) return nullptr;
    return *process_group_ptr;
}

sched::session *ns::pid::get_session(pid_t sid) {
    util::lock_guard guard{session_lock};

    auto session_ptr = session_tree.find(sid);
    if (session_ptr == nullptr) return nullptr;
    return *session_ptr;
}