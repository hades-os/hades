#ifndef VMM_HPP
#define VMM_HPP

#include <stdint.h>
#include <stddef.h>
#include <mm/pmm.hpp>
#include <util/lock.hpp>
#include <frg/vector.hpp>
#include <frg/rbtree.hpp>

#define VMM_PRESENT (1 << 0)
#define VMM_WRITE (1 << 1)
#define VMM_USER (1 << 2)
#define VMM_DIRTY (1 << 5)
#define VMM_LARGE (1 << 7)
#define VMM_PALLOC (1 << 9)
#define VMM_SHARED (1 << 10)

#define VMM_IS_USER(x) (((uint64_t) x) < MEM_BASE)

#define VMM_ENTRIES_PER_TABLE 512
#define VMM_ADDR_MASK ~0x8000000000000FFF

#define VMM_4GIB   (4294967296)

#define PG_WRITE   (1 << 3)
#define PG_NONE    (1 << 4)
#define PG_READ    (1 << 5)
#define PG_EXEC    (1 << 6)

#define PG_LARGE   (1 << 12)

#define PG_SHARED  (1 << 7)
#define PG_PRIVATE (1 << 8)

#define PG_ANON    (1 << 0)
#define PG_FIXED   (1 << 11)

#define PG_OVERRIDE (1 << 14)

#define PG_KERN (PG_LARGE | PG_WRITE | PG_RAW | PG_OVERRIDE)

namespace memory {
    namespace vmm {
            class vmm_ctx {
                private:            
                    struct hole {
                        public:
                            void *addr = nullptr;
                            uint64_t len = 0;
                            uint64_t largest_hole = 0;
                            void *map = nullptr;

                            frg::rbtree_hook hook;

                            hole(void *addr, uint64_t len, void *map) {
                                this->addr = addr;
                                this->len = len;
                                this->largest_hole = 0;
                                this->map = map;
                            };
                    };

                    struct hole_comparator {
                        bool operator() (hole& a, hole& b) {
                            return a.addr < b.addr;
                        };
                    };

                    struct hole_aggregator;
                    using hole_tree = frg::rbtree<hole, &hole::hook, hole_comparator, hole_aggregator>;

                    struct hole_aggregator {
                        static bool aggregate(hole *node);
                        static bool check_invariant(hole_tree& tree, hole *node);
                    };

                    void *split_hole(hole *node, uint64_t offset, size_t len);
                public:
                    uint64_t *map = nullptr;
                    hole_tree holes;

                    struct mapping {
                        public:
                            void *addr = nullptr;
                            uint64_t len = 0;
                            void *map = nullptr;
                            uint64_t perms = 0;
                            bool huge_page;
                            bool fault_map;
                            bool is_unmanaged;
                            frg::rbtree_hook hook;
                            struct callback_obj {
                                void *(*map)(void *virt, bool huge_page, void *map) = nullptr;
                                void (*unmap)(void *virt, bool huge_page, void *map) = nullptr;
                            };
                            mapping::callback_obj callbacks;

                            mapping(void *addr, uint64_t len, void *map, bool huge_page) : addr(addr), len(len), map(map), huge_page(huge_page), fault_map(false), is_unmanaged(false) { };

                            mapping(void *addr, uint64_t len, void *map, bool huge_page, mapping::callback_obj callbacks) : mapping(addr, len, map, huge_page) {
                                this->callbacks = callbacks;
                                this->fault_map = true;
                            };
                    };

                    void *create_hole(void *addr, uint64_t len);
                    uint8_t delete_hole(void *addr, uint64_t len);

                    void *unmanaged_mapping(void *addr, uint64_t len, uint64_t flags);
                    void *create_mapping(void *addr, uint64_t len, uint64_t flags);
                    void *create_mapping(void *addr, uint64_t len, uint64_t flags, mapping::callback_obj callbacks);

                    void *delete_mapping(void *addr, uint64_t len);
                private:
                    struct mapping_comparator {
                        bool operator() (mapping& a, mapping& b) {
                            return a.addr < b.addr;
                        };
                    };

                    using mapping_tree = frg::rbtree<mapping, &mapping::hook, mapping_comparator, frg::null_aggregator>;
                    mapping_tree mappings;

                    uint8_t mapped(void *addr, uint64_t len);
            };

            namespace common {
                inline int64_t *refs = nullptr;
                inline uint64_t refs_len = 0;
                inline vmm_ctx *boot_ctx = nullptr;
                inline util::lock lock{};
            };

            namespace x86 {
                void *_map(void *phys, void *virt, uint64_t flags, void *ptr);
                void *_map2(void *phys, void *virt, uint64_t flags, void *ptr);

                void *_unmap(void *virt, void *ptr);
                void *_unmap2(void *virt, void *ptr);

                void *_perms(void *virt, uint64_t flags, void *ptr);
                void *_perms2(void *virt, uint64_t flags, void *ptr);

                bool _valid(uint64_t flags, void *virt, void *phys);
                int64_t _filter(uint64_t flags);

                void _ref(void *ptr);
                void _ref(void *ptr, uint64_t len);

                void _free(void *ptr);
                void _free(void *ptr, uint64_t len);

                void *_virt(uint64_t phys);
                void *_virt(void* phys);
                void *_phys(uint64_t virt);
                void *_phys(void *virt);
            };

            // real functions

            void init();
            void *create();
            void destroy(void *ptr);
            void change(void *ptr);
            void *boot();
            void *cr3(void *ptr);

            void *map(void *virt, uint64_t len, uint64_t flags, void *ptr);
            void *map(void *virt, uint64_t len, uint64_t flags, void *ptr, vmm::vmm_ctx::mapping::callback_obj callbacks);
            void *map(void *phys, void *virt, uint64_t len, uint64_t flags, void *ptr);
            void *unmap(void *virt, uint64_t len, void *ptr);

            void *fork(void *ptr);            
    };
};

#endif