#ifndef LOCK_HPP
#define LOCK_HPP

#include <sys/irq.hpp>
#include <cstdint>

namespace util {
    class lock {
        private:
            volatile bool _lock;
            bool interrupts;
            bool get_interrupts() {
                uint64_t rflags = 0;
                asm volatile ("pushfq; \
                               pop %0 "
                               : "=r"(rflags)
                );

                return (rflags >> 9) & 1;
            }
        public:
            void acquire() {
                while(__atomic_test_and_set(&this->_lock, __ATOMIC_ACQUIRE));
            }
            
            void irq_acquire() {
                interrupts = get_interrupts();
                irq::off();
                while(__atomic_test_and_set(&this->_lock, __ATOMIC_ACQUIRE));
            }

            void release() {
                __atomic_clear(&this->_lock, __ATOMIC_RELEASE);
            }

            void irq_release() {
                __atomic_clear(&this->_lock, __ATOMIC_RELEASE);
                if (interrupts) {
                    irq::on();
                } else {
                    irq::off();
                }
            }

            void await() {
                acquire();
                release();
            }

            lock() : _lock(0) { }

            lock(const lock &other) = delete;

            ~lock() {
                this->release();
            }
    };
};

#endif