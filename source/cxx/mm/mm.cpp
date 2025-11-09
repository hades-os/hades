#include <cstddef>
#include <cstdint>
#include <mm/mm.hpp>
#include <mm/pmm.hpp>
#include <util/lock.hpp>
#include <util/string.hpp>

#define ALIGNMENT	16ul//4ul				///< This is the byte alignment that memory must be allocated on. IMPORTANT for GTK and other stuff.

/** This macro will conveniently align our pointer upwards */
#define ALIGN( ptr )													\
		if ( ALIGNMENT > 1 )											\
		{																\
			uintptr_t diff;												\
			ptr = (void*)((uintptr_t)ptr + (sizeof(char) * 16));					\
			diff = (uintptr_t)ptr & (ALIGNMENT-1);						\
			if ( diff != 0 )											\
			{															\
				diff = ALIGNMENT - diff;								\
				ptr = (void*)((uintptr_t)ptr + diff);					\
			}															\
			*((char*)((uintptr_t)ptr - (sizeof(char) * 16))) = 			\
				diff + (sizeof(char) * 16);										\
		}

#define UNALIGN( ptr )													\
		if ( ALIGNMENT > 1 )											\
		{																\
			uintptr_t diff = *((char*)((uintptr_t)ptr - (sizeof(char) * 16)));	\
			if ( diff < (ALIGNMENT + (sizeof(char) * 16)) )						\
			{															\
				ptr = (void*)((uintptr_t)ptr - diff);					\
			}															\
		}

constexpr size_t LIBALLOC_MAGIC	= 0xc001c0de;
constexpr size_t LIBALLOC_DEAD	= 0xdeaddead;

struct liballoc_minor;
/** A structure found at the top of all system allocated
 * memory blocks. It details the usage of the memory ballocLock.
 */
struct liballoc_major {
    liballoc_major *prev;		///< Linked list information.
    liballoc_major *next;		///< Linked list information.
	unsigned int pages;					///< The number of pages in the ballocLock.
	unsigned int size;					///< The number of pages in the ballocLock.
	unsigned int usage;					///< The number of bytes used in the ballocLock.
	liballoc_minor *first;		///< A pointer to the first allocated memory in the ballocLock.
};


/** This is a structure found at the beginning of all
 * sections in a major block which were allocated by a
 * malloc, calloc, realloc call.
 */
struct	liballoc_minor {
	liballoc_minor *prev;		///< Linked list information.
	liballoc_minor *next;		///< Linked list information.
	liballoc_major *block;		///< The owning ballocLock. A pointer to the major structure.
	unsigned int magic;					///< A magic number to idenfity correctness.
	unsigned int size; 					///< The size of the memory allocated. Could be 1 byte or more.
	unsigned int req_size;				///< The size of memory requested.
};

static liballoc_major *memRoot = nullptr;	///< The root memory block acquired from the system.
static liballoc_major *bestBet = nullptr; ///< The major with the most free memory.

static size_t pageCount = 16;			///< The number of pages to request per chunk. Set up in liballoc_init.
static size_t allocated = 0;		///< Running total of allocated memory.
static size_t inuse	 = 0;		///< Running total of used memory.

static util::lock allocLock{};

static liballoc_major *allocate_new_page(size_t size) {
	size_t st;
	liballoc_major *maj;

    // This is how much space is required.
    st  = size + sizeof(liballoc_major);
    st += sizeof(liballoc_minor);

            // Perfect amount of space?
    if ((st % memory::common::page_size) == 0) {
        st  = st / (memory::common::page_size);
    } else {
        st  = st / (memory::common::page_size) + 1;
    }

    // Make sure it's >= the minimum size.
    if (st < pageCount) {
        st = pageCount;
    }

    maj = (liballoc_major *) memory::pmm::alloc(st);

    if (maj == nullptr) {
        return nullptr;	// uh oh, we ran out of memory.
    }

    maj->prev  = nullptr;
    maj->next  = nullptr;
    maj->pages = st;
    maj->size  = st * memory::common::page_size;
    maj->usage = sizeof(liballoc_major);
    maj->first = nullptr;

    allocated += maj->size;

    return maj;
}

namespace memory {
    namespace mm {
        namespace allocator {
            void *malloc(size_t req_size) {
                int startedBet = 0;
                size_t bestSize = 0;
                void *p = nullptr;
                uintptr_t diff;
                struct liballoc_major *maj;
                struct liballoc_minor *min;
                struct liballoc_minor *new_min;
                size_t size = req_size;

                // For alignment, we adjust size so there's enough space to align.
                if (ALIGNMENT > 1) {
                    size += ALIGNMENT + (sizeof(char) * 16);
                }

                allocLock.acquire();

                if (size == 0) {
                    return malloc(1);
                }

                if (memRoot == nullptr) {
                    // This is the first time we are being used.
                    memRoot = allocate_new_page(size);
                    if (memRoot == nullptr) {
                        allocLock.release();
                        return nullptr;
                    }
                }

                // Now we need to bounce through every major and find enough space....
                maj = memRoot;
                startedBet = 0;

                // Start at the best bet....
                if (bestBet != nullptr) {
                    bestSize = bestBet->size - bestBet->usage;

                    if (bestSize > (size + sizeof(struct liballoc_minor))) {
                        maj = bestBet;
                        startedBet = 1;
                    }
                }

                while (maj != nullptr) {
                    diff  = maj->size - maj->usage; // free memory in the block

                    if (bestSize < diff) {
                        // Hmm.. this one has more memory then our bestBet. Remember!
                        bestBet = maj;
                        bestSize = diff;
                    }

                    // CASE 1:  There is not enough space in this major ballocLock.
                    if (diff < (size + sizeof(liballoc_minor))) {
                        // Another major block next to this one?
                        if (maj->next != nullptr) {
                            maj = maj->next;		// Hop to that one.
                            continue;
                        }

                        if (startedBet == 1) { // If we started at the best bet,  let's start all over again.
                            maj = memRoot;
                            startedBet = 0;
                            continue;
                        }

                        // Create a new major block next to this one and...
                        maj->next = allocate_new_page(size);	// next one will be okay.
                        if (maj->next == nullptr) break;			// no more memory.
                        maj->next->prev = maj;
                        maj = maj->next;
                    }

                    // CASE 2: It's a brand new ballocLock.
                    if (maj->first == nullptr) {
                        maj->first = (liballoc_minor *) ((uintptr_t) maj + sizeof(liballoc_major));

                        maj->first->magic 		= LIBALLOC_MAGIC;
                        maj->first->prev 		= nullptr;
                        maj->first->next 		= nullptr;
                        maj->first->block 		= maj;
                        maj->first->size 		= size;
                        maj->first->req_size 	= req_size;
                        maj->usage 	+= size + sizeof( struct liballoc_minor );

                        inuse += size;

                        p = (void *) ((uintptr_t) (maj->first) + sizeof(liballoc_minor));
                        ALIGN(p);

                        allocLock.release(); // release the lock
                        return p;
                    }

                    // CASE 3: Block in use and enough space at the start of the ballocLock.
                    diff =  (uintptr_t) (maj->first);
                    diff -= (uintptr_t) maj;
                    diff -= sizeof(liballoc_major);

                    if (diff >= (size + sizeof(liballoc_minor))) {
                        // Yes, space in front. Squeeze in.
                        maj->first->prev = (liballoc_minor *) ((uintptr_t) maj + sizeof(liballoc_major));
                        maj->first->prev->next = maj->first;
                        maj->first = maj->first->prev;

                        maj->first->magic 	 = LIBALLOC_MAGIC;
                        maj->first->prev 	 = nullptr;
                        maj->first->block 	 = maj;
                        maj->first->size 	 = size;
                        maj->first->req_size = req_size;
                        maj->usage 			+= size + sizeof(liballoc_minor);

                        inuse += size;

                        p = (void *) ((uintptr_t) (maj->first) + sizeof(liballoc_minor));
                        ALIGN(p);

                        allocLock.release();		// release the lock
                        return p;
                    }

                    // CASE 4: There is enough space in this ballocLock. But is it contiguous?
                    min = maj->first;

                    // Looping within the block now...
                    while ( min != nullptr ) {
                        // CASE 4.1: End of minors in a ballocLock. Space from last and end?
                        if (min->next == nullptr) {
                            // the rest of this block is free...  is it big enough?
                            diff  = (uintptr_t) (maj) + maj->size;
                            diff -= (uintptr_t) min;
                            diff -= sizeof(liballoc_minor);
                            diff -= min->size;
                            // minus already existing usage..

                            if (diff >= (size + sizeof(liballoc_minor))) {
                                // yay....
                                min->next = (liballoc_minor *)((uintptr_t) min + sizeof(liballoc_minor) + min->size);
                                min->next->prev = min;
                                min = min->next;
                                min->next = nullptr;
                                min->magic = LIBALLOC_MAGIC;
                                min->block = maj;
                                min->size = size;
                                min->req_size = req_size;
                                maj->usage += size + sizeof(liballoc_minor);

                                inuse += size;

                                p = (void *) ((uintptr_t) min + sizeof(liballoc_minor));
                                ALIGN(p);

                                allocLock.release();		// release the lock
                                return p;
                            }
                        }

                        // CASE 4.2: Is there space between two minors?
                        if (min->next != nullptr) {
                            // is the difference between here and next big enough?
                            diff  = (uintptr_t) (min->next);
                            diff -= (uintptr_t) min;
                            diff -= sizeof(liballoc_minor);
                            diff -= min->size;
                            // minus our existing usage.

                            if (diff >= (size + sizeof(liballoc_minor))) {
                                // yay......
                                new_min = (liballoc_minor *) ((uintptr_t) min + sizeof(liballoc_minor) + min->size);

                                new_min->magic = LIBALLOC_MAGIC;
                                new_min->next = min->next;
                                new_min->prev = min;
                                new_min->size = size;
                                new_min->req_size = req_size;
                                new_min->block = maj;
                                min->next->prev = new_min;
                                min->next = new_min;
                                maj->usage += size + sizeof(liballoc_minor);

                                inuse += size;

                                p = (void *) ((uintptr_t) new_min + sizeof(liballoc_minor));
                                ALIGN(p);

                                allocLock.release(); // release the lock
                                return p;
                            }
                        }

                        min = min->next;
                    }

                    // CASE 5: Block full! Ensure next block and loop.
                    if (maj->next == nullptr) {
                        if (startedBet == 1) {
                            maj = memRoot;
                            startedBet = 0;
                            continue;
                        }

                        // we've run out. we need more...
                        maj->next = allocate_new_page(size); // next one guaranteed to be okay
                        if (maj->next == nullptr) { //  uh oh,  no more memory...
                            break; 
                        }

                        maj->next->prev = maj;
                    }

                    maj = maj->next;
                }

                allocLock.release(); // release the lock

                return nullptr;
            }

            void free(void *ptr) {
                liballoc_minor *min;
                liballoc_major *maj;

                if (ptr == nullptr) {
                    return;
                }

                UNALIGN(ptr);

                allocLock.acquire();		// lockit

                min = (liballoc_minor *) ((uintptr_t) ptr - sizeof(liballoc_minor));

                if (min->magic != LIBALLOC_MAGIC) {
                    allocLock.release(); // release the lock
                    return;
                }

                maj = min->block;

                inuse -= min->size;

                maj->usage -= (min->size + sizeof(liballoc_minor));
                min->magic  = LIBALLOC_DEAD;		// No mojo.

                if (min->next != nullptr) {
                    min->next->prev = min->prev; 
                }
                if (min->prev != nullptr) {
                    min->prev->next = min->next; 
                }

                if (min->prev == nullptr) { // Might empty the ballocLock. This was the first minor.
                    maj->first = min->next;
                }

                // We need to clean up after the majors now....
                if (maj->first == nullptr) {// Block completely unused.
                    if (memRoot == maj) {
                        memRoot = maj->next; 
                    }

                    if (bestBet == maj) {
                        bestBet = nullptr; 
                    }

                    if (maj->prev != nullptr) {
                        maj->prev->next = maj->next; 
                    }

                    if (maj->next != nullptr) {
                        maj->next->prev = maj->prev; 
                    }

                    allocated -= maj->size;

                    memory::pmm::free(maj, maj->pages);
                } else {
                    if (bestBet != nullptr) {
                        int bestSize = bestBet->size - bestBet->usage;
                        int majSize = maj->size - maj->usage;

                        if (majSize > bestSize) {
                            bestBet = maj;
                        }
                    }
                }

                allocLock.release();		// release the lock
            }

            void *calloc(size_t nr_items, size_t size) {
                int real_size;
                void *p;

                real_size = nr_items * size;

                p = malloc(real_size);

                memset(p, 0, real_size);
                return p;
            }

            void *realloc(void *p, size_t size) {
                void *ptr;
                liballoc_minor *min;
                size_t real_size;

                // Honour the case of size == 0 => free old and return nullptr
                if (size == 0) {
                    free(p);
                    return nullptr;
                }

                // In the case of a nullptr pointer, return a simple malloc.
                if (p == nullptr) {
                    return malloc(size);
                }

                // Unalign the pointer if required.
                ptr = p;
                UNALIGN(ptr);

                allocLock.acquire();		// lockit

                min = (liballoc_minor *)((uintptr_t) ptr - sizeof(liballoc_minor));

                // Ensure it is a valid structure.
                if (min->magic != LIBALLOC_MAGIC) {
                    // being lied to...
                    allocLock.release(); // release the lock
                    return nullptr;
                }

                // Definitely a memory ballocLock.
                real_size = min->req_size;
                if (real_size >= size) {
                    min->req_size = size;
                    allocLock.release();
                    return p;
                }

                allocLock.release();

                // If we got here then we're reallocating to a block bigger than us.
                ptr = malloc(size); // We need to allocate new memory
                memcpy(ptr, p, real_size);
                free(p);

                return ptr;
            }
        };
    };
};