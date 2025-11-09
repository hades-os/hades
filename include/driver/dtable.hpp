#ifndef DTABLE_HPP
#define DTABLE_HPP

#include <cstddef>
#include <fs/dev.hpp>
#include <frg/allocation.hpp>
#include <driver/matchers.hpp>
#include <driver/majtable.hpp>
#include <mm/boot.hpp>

// match_data is bus-specific

namespace dtable {
    constexpr size_t MATCH_ANY = 0xFFFF;
    struct entry {
        int match_data[16];
        ssize_t major;
        vfs::devfs::matcher *matcher;
    };

    static entry entries[] = {
        { .match_data = {MATCH_ANY, MATCH_ANY, 0x1, 0x6, 0x1}, .major = majors::AHCIBUS, .matcher = frg::construct<pci::ahcibus::matcher>(mm::boot())},
        { .match_data = {0}, .major=majors::AHCI, .matcher = frg::construct<ahci::matcher>(mm::boot())},
        { .match_data = {0x8086, 0x100e, MATCH_ANY, MATCH_ANY, MATCH_ANY}, .major = majors::NET, .matcher = frg::construct<pci::net::matcher>(mm::boot())},
        { .match_data = {0}, .major=majors::FB, .matcher = frg::construct<fb::matcher>(mm::boot())},
        { .match_data = {0}, .major=majors::KB, .matcher = frg::construct<kb::matcher>(mm::boot())},
        { .match_data = {0}, .major=majors::PTM, .matcher = frg::construct<tty::ptm::matcher>(mm::boot())},
        { .match_data = {0}, .major=majors::PTS, .matcher = frg::construct<tty::pts::matcher>(mm::boot())},
        { .match_data = {0}, .major=majors::PTMX, .matcher = frg::construct<tty::ptmx::matcher>(mm::boot())},
        { .match_data = {0}, .major=majors::SELF_TTY, .matcher = frg::construct<tty::self::matcher>(mm::boot())},
        { .match_data = {0}, .major=majors::VT, .matcher = frg::construct<vt::matcher>(mm::boot())}
    };

    vfs::devfs::matcher *lookup_by_data(int *match_data, size_t len);
    vfs::devfs::matcher *lookup_by_major(ssize_t major);
}

#endif