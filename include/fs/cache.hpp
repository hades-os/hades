#ifndef CACHE_HPP
#define CACHE_HPP

#include <cstddef>
#include <cstdint>
#include <fs/dev.hpp>
#include <frg/rcu_radixtree.hpp>
#include <frg/tuple.hpp>
#include <ipc/link.hpp>
#include <mm/mm.hpp>
#include <util/types.hpp>
#include "mm/arena.hpp"
#include "prs/allocator.hpp"
#include "prs/list.hpp"

namespace cache {
    void halt_sync();
    void sync_worker();
    void init();

    class holder {
        private:
            util::spinlock lock;

            prs::allocator allocator;

            frg::rcu_radixtree<uintptr_t, prs::allocator> address_tree;
            vfs::devfs::blockdev *backing_device;

            frg::tuple<ssize_t, void*> write_page(size_t offset );
            frg::tuple<ssize_t, void*> read_page(size_t offset);
            ssize_t release_page(size_t offset);

            ssize_t request_page(void *buffer, size_t offset, size_t buffer_len, size_t buffer_offset, size_t page_offset, bool rw);

            ipc::link link;

            struct request {
                size_t offset;
                size_t page_offset;

                size_t buffer_len;
                size_t buffer_offset;
                void *buffer;

                ssize_t error;
                bool rw;
                size_t link_id;
            };

            size_t pending_reads;
            size_t pending_writes;

            bool syncing;

            prs::vector<shared_ptr<request>, prs::allocator> requests;
        public:
            ssize_t request_io(void *buffer, size_t offset, size_t len, bool rw);

            int free_pages();
            int sync_pages();

            void halt_syncing();

            prs::list_hook hook;
            holder(vfs::devfs::blockdev *backing_device): 
                lock(), allocator(arena::create_resource()), 
                address_tree(allocator), backing_device(backing_device),
                link(),
                pending_reads(0), pending_writes(0), 
                syncing(true), requests(allocator),
                hook() {} 

            friend void sync_worker();
    };

    holder *create_cache(vfs::devfs::blockdev *backing_device);
}

#endif