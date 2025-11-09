#include "arch/x86/smp.hpp"
#include "arch/x86/types.hpp"
#include "frg/intrusive.hpp"
#include "frg/list.hpp"
#include "ipc/evtable.hpp"
#include "mm/common.hpp"
#include "mm/slab.hpp"
#include "mm/vmm.hpp"
#include "prs/list.hpp"
#include "smarter/smarter.hpp"
#include "sys/sched/sched.hpp"
#include "sys/sched/time.hpp"
#include "util/lock.hpp"
#include "util/log/log.hpp"
#include "util/types.hpp"
#include <util/log/panic.hpp>
#include <cstddef>
#include <fs/cache.hpp>
#include <frg/vector.hpp>
#include <mm/mm.hpp>
#include <mm/pmm.hpp>

static prs::list<
    cache::holder,
    &cache::holder::hook
>
caches{};

static bool syncing = true;
static log::subsystem logger = log::make_subsystem("CACHE");

frg::tuple<
    ssize_t,
    void *
> cache::holder::read_page(size_t offset) {
    util::lock_guard guard{lock};

    uintptr_t *page = address_tree.find(offset);
    void *out_page = page == nullptr ? pmm::alloc(1) : (void *) *page;
    if (page == nullptr) {
        address_tree.insert(offset, (uintptr_t) out_page);

        auto res = backing_device->read(out_page, memory::page_size, offset);
        if (res < 0) {
            pmm::free(out_page);
            return {-1, nullptr};
        }
    }

    return {0, out_page};
}

frg::tuple<
    ssize_t,
    void *
> cache::holder::write_page(size_t offset) {
    util::lock_guard guard{lock};

    uintptr_t *page = address_tree.find(offset);
    void *out_page = page == nullptr ? pmm::alloc(1) : (void *) *page;

    ssize_t res;
    if (page == nullptr) {
        address_tree.insert(offset, (uintptr_t) out_page);
        res = backing_device->read(out_page, memory::page_size, offset);
    } else {
        res = backing_device->write(out_page, memory::page_size, offset);
    }

    if (res < 0) {
        pmm::free(out_page);
        return {-1, nullptr};
    }

    return {0, out_page};
}

// TODO: add disk flushing
ssize_t cache::holder::release_page(size_t offset) {
    util::lock_guard guard{lock};

    uintptr_t *page = address_tree.find(offset);
    if (page == nullptr) return -1;

    backing_device->write((void *) *page, memory::page_size, offset);
    pmm::free((void *) (*page));

    return 0;
}

ssize_t cache::holder::request_page(void *buffer, size_t offset, size_t buffer_len, size_t buffer_offset, size_t page_offset, bool rw) {
    if (page_offset + buffer_len > memory::page_size) {
        panic("Buffer overrun");
    }

    if (uintptr_t *page = address_tree.find(offset)) {
        void *in_page = (void *) *page;
        if (rw) {
            memcpy((char *) in_page + page_offset, (char *) buffer + buffer_offset, buffer_len);
            write_page(offset);
        } else {
            memcpy((char *) buffer + buffer_offset, (char *) in_page + page_offset, buffer_len);
        }

        return false;
    }

    shared_ptr<holder::request> req = prs::allocate_shared<holder::request>(mm::slab<holder::request>());

    req->offset = offset;
    req->page_offset = page_offset;

    req->buffer = buffer;
    req->buffer_offset = buffer_offset;
    req->buffer_len = buffer_len;

    req->error = 0;
    req->rw = rw;

    size_t id = link.request(rw ? evtable::BLOCK_WRITE : evtable::BLOCK_READ,
        [&](size_t id) {
        req->link_id = id;

        requests.push(req);
        req->rw ? pending_writes++ : pending_reads++;
    });

    link.sync_wait(evtable::BLOCK_FIN, id, true);
    return req->error;
}

ssize_t cache::holder::request_io(void *buffer, size_t offset, size_t len, bool rw) {
    for (uint64_t headway = 0; headway < len;) {
        size_t page = (offset + headway) & ~(0xFFF);
        size_t page_offset = (offset + headway) & 0xFFF;

        size_t length = len - headway;
        if (length > (memory::page_size - page_offset)) {
            length = memory::page_size - page_offset;
        }

        if (headway > len) {
            panic("Buffer overflow");
        }

        auto res = request_page(buffer, page, length, headway, page_offset, rw);
        if (res < 0) {
            return -1;
        }

        headway += length;
    }

    return 0;
}

cache::holder *cache::create_cache(vfs::devfs::blockdev *backing_device) {
    auto cache = frg::construct<cache::holder>(mm::slab<cache::holder>(), backing_device);
    return *caches.push_back(cache);
}

void cache::halt_sync() {
    __atomic_clear(&syncing, __ATOMIC_RELEASE);
}

void cache::sync_worker() {
    while (syncing) {
        for (auto holder: caches) {
            if (holder->requests.size() == 0) continue;
            if (!holder->syncing) continue;

            sched::timespec timeout = sched::timespec::ms(1000);

            holder->link.recv({ evtable::BLOCK_READ, evtable::BLOCK_WRITE },
                true, &timeout);

            auto request = holder->requests.pop();

            if (request->rw) {
                auto [res, page] = holder->read_page(request->offset);
                if (res < 0) {
                    request->error = res;
                } else {
                    memcpy((char *) page + request->page_offset, (char *) request->buffer + request->buffer_offset, request->buffer_len);
                    holder->write_page(request->offset);    
                }
            } else {
                auto [res, page] = holder->read_page(request->offset);
                if (res < 0) {
                    request->error = res;
                } else {
                    memcpy((char *) request->buffer + request->buffer_offset, (char *) page + request->page_offset, request->buffer_len);
                }
            }

            request->rw ? holder->pending_writes-- : holder->pending_reads--;
            holder->link.reply(evtable::BLOCK_FIN, request->link_id);
        }
    }
}

sched::thread *sync_thread;
void cache::init() {
    sync_thread = sched::create_thread(sync_worker, (uint64_t) pmm::stack(x86::initialStackSize), vmm::boot, 0);
    sync_thread->start();
}