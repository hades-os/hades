#include <cstddef>
#include <frg/allocation.hpp>
#include <frg/macros.hpp>
#include <mm/mm.hpp>
#include <util/log/log.hpp>
#include <new>

extern "C" {
	void FRG_INTF(log)(const char *cstring) {
        kmsg("[FRG | INFO]: ", cstring);
    }

	void FRG_INTF(panic)(const char *cstring) {
        panic("[FRG | PANIC]: ", cstring);
    }
}

void operator delete(void *ptr) {
    kfree(ptr);
}

void operator delete(void *ptr, size_t _) {
    kfree(ptr);
}

void *operator new(size_t size) {
    return kmalloc(size);
}

