#ifndef LOCK_HPP
#define LOCK_HPP

#include <cstdint>

extern "C" {
    void atomic_lock(uint64_t *lock);
    void atomic_unlock(uint64_t *lock);
}

namespace util {
    class lock {
        private:
            uint64_t _lock;
        public:
            void acquire() {
                atomic_lock(&this->_lock);
            }

            void release() {
                atomic_unlock(&this->_lock);
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