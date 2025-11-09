#ifndef VMM_HPP
#define VMM_HPP

#include <stdint.h>
#include <stddef.h>
#include <mm/pmm.hpp>
#include <util/lock.hpp>
#include <frg/vector.hpp>
#include <frg/rbtree.hpp>
#include <sys/irq.hpp>

#define VMM_PRESENT (1 << 0)
#define VMM_WRITE (1 << 1)
#define VMM_USER (1 << 2)
#define VMM_LARGE (1 << 7)

#define VMM_FIXED (1 << 8)
#define VMM_MANAGED (1 << 9)
#define VMM_SHARED (1 << 10)
#define VMM_FILE (1 << 11)

#define VMM_EXECUTE (1UL << 63)

#define VMM_ENTRIES_PER_TABLE 512
#define VMM_ADDR_MASK ~0x8000000000000FFF

#define VMM_4GIB   (4294967296)

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

                    util::lock lock;

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
                                bool (*map)(void *virt, bool huge_page, void *map) = nullptr;
                                bool (*unmap)(void *virt, bool huge_page, void *map) = nullptr;
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
                    void *split_mapping(void *addr, uint64_t len);
                    void *create_mapping(void *addr, uint64_t len, uint64_t flags);
                    void *create_mapping(void *addr, uint64_t len, uint64_t flags, mapping::callback_obj callbacks);

                    mapping *get_mapping(void *addr);
                    mapping *get_mappings() { return this->mappings.first(); };
                    mapping *get_next(mapping *node) { return this->mappings.successor(node); };

                    void copy_mappings(vmm_ctx *other);

                    void delete_mapping(mapping *node);
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
                inline util::lock vmm_lock{};
            };

            namespace x86 {
                void *_map(void *phys, void *virt, uint64_t flags, void *ptr);
                void *_map2(void *phys, void *virt, uint64_t flags, void *ptr);

                void *_get(void *virt, void *ptr);
                void *_get2(void *virt, void *ptr);

                void *_unmap(void *virt, void *ptr);
                void *_unmap2(void *virt, void *ptr);

                void *_perms(void *virt, uint64_t flags, void *ptr);
                void *_perms2(void *virt, uint64_t flags, void *ptr);

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

            uint64_t read_cr3();
            void write_cr3(uint64_t map);
            bool handle_pf(irq::regs *r);

            void *map(void *virt, uint64_t len, uint64_t flags, void *ptr);
            void *map(void *virt, uint64_t len, uint64_t flags, void *ptr, vmm::vmm_ctx::mapping::callback_obj callbacks);
            void *map(void *phys, void *virt, uint64_t len, uint64_t flags, void *ptr);
            void *resolve(void *virt, void *ptr);
            void *unmap(void *virt, uint64_t len, void *ptr);

            void *fork(void *ptr);            
    };
};

#endif