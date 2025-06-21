#ifndef DTABLE_HPP
#define DTABLE_HPP

#include <cstddef>
#include <driver/matchers.hpp>
#include <driver/majtable.hpp>
#include <fs/dev.hpp>
#include <prs/construct.hpp>
#include <mm/arena.hpp>
#include <prs/allocator.hpp>

// match_data is bus-specific

namespace dtable {
    constexpr size_t MATCH_ANY = 0xFFFF;
    struct entry {
        int match_data[16];
        ssize_t major;
        vfs::devfs::matcher *matcher;
    };
    
    static prs::allocator allocator{
        arena::create_resource()
    };
    static entry entries[] = {
        { .match_data = {MATCH_ANY, MATCH_ANY, 0x1, 0x6, 0x1}, .major = majors::AHCIBUS, .matcher = prs::construct<pci::ahcibus::matcher>(allocator)},
        { .match_data = {0}, .major=majors::AHCI, .matcher = prs::construct<ahci::matcher>(allocator)},
        { .match_data = {0x8086, 0x100e, MATCH_ANY, MATCH_ANY, MATCH_ANY}, .major = majors::NET, .matcher = prs::construct<pci::net::matcher>(allocator)},
        { .match_data = {0}, .major=majors::FB, .matcher = prs::construct<fb::matcher>(allocator)},
        { .match_data = {0}, .major=majors::KB, .matcher = prs::construct<kb::matcher>(allocator)},
        { .match_data = {0}, .major=majors::PTM, .matcher = prs::construct<tty::ptm::matcher>(allocator)},
        { .match_data = {0}, .major=majors::PTS, .matcher = prs::construct<tty::pts::matcher>(allocator)},
        { .match_data = {0}, .major=majors::PTMX, .matcher = prs::construct<tty::ptmx::matcher>(allocator)},
        { .match_data = {0}, .major=majors::SELF_TTY, .matcher = prs::construct<tty::self::matcher>(allocator)},
        { .match_data = {0}, .major=majors::VT, .matcher = prs::construct<vt::matcher>(allocator)}
    };

    vfs::devfs::matcher *lookup_by_data(int *match_data, size_t len);
    vfs::devfs::matcher *lookup_by_major(ssize_t major);
}

#endif