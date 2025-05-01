#ifndef PRS_SHARED_HPP
#define PRS_SHARED_HPP

#include <atomic>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <cstddef>

#include "prs/assert.hpp"
#include "prs/deleter.hpp"
#include "prs/unique.hpp"

namespace prs {
    struct control_base {
        public:
            virtual ~control_base() = default;

            virtual void increment_use() noexcept = 0;
            virtual void increment_weak() noexcept = 0;
            virtual void decrement_use() noexcept = 0;
            virtual void decrement_weak() noexcept = 0;

            virtual long use_count() const noexcept = 0;
            virtual long weak_use_count() const noexcept = 0;

            virtual bool unique() const noexcept = 0;
            virtual bool expired() const noexcept = 0;
    };

    template<typename T, typename Allocator, typename D = default_deleter<T, Allocator>>
    struct control: public control_base {
        private:
            T *ptr;
            D deleter;
            Allocator alloc;

            std::atomic_long _use_count;
            std::atomic_long _weak_count;
        public:
            control(T *p, Allocator alloc):
                ptr(p), deleter(alloc), alloc(std::move(alloc)),
                _use_count(1), _weak_count(1) {}

            control(T *p, D d, Allocator alloc):
                ptr(p), deleter(d), alloc(std::move(alloc)),
                _use_count(1), _weak_count(1) {}

            ~control()
                {}

            void increment_use() noexcept override {
                _use_count.fetch_add(1, std::memory_order_acq_rel);
            }

            void increment_weak() noexcept override {
                _weak_count.fetch_add(1, std::memory_order_acq_rel);
            }

            void decrement_use() noexcept override {
                if (_use_count.fetch_sub(1, std::memory_order_acq_rel) == 0) {
                    if (ptr) {
                        deleter(ptr);
                        ptr = nullptr;
                    }

                    decrement_weak();
                }
            }

            void decrement_weak() noexcept override {
                if (_weak_count.fetch_sub(1, std::memory_order_acq_rel) == 0) {
                    alloc.free(this);
                    this->~control();
                }
            }

            long use_count() const noexcept override {
                return _use_count;
            }

            bool unique() const noexcept override {
                return _use_count == 1;
            }

            long weak_use_count() const noexcept override {
                return _weak_count - ((_use_count > 0) ? 1 : 0);
            }

            bool expired() const noexcept override {
                return _use_count == 0;
            }
    };

    template<typename T> struct shared_ptr;
    template<typename T> struct weak_ptr;

    template<typename T>
    struct shared_ptr {
        private:
            T *ptr;
            control_base *ctl;
        public:
            using element_type = T;
            using weak_type = weak_ptr<T>;

            template<typename U>
            friend struct shared_ptr;

            template<typename U>
            friend struct weak_ptr;

            constexpr shared_ptr() noexcept:
                ptr(), ctl() {}

            constexpr shared_ptr(std::nullptr_t) noexcept:
                ptr(), ctl() {}

            template<typename U, typename Allocator>
            explicit shared_ptr(U *ptr, Allocator alloc):
                ptr(ptr),
                ctl(new (alloc.allocate(sizeof(control<T, Allocator>)))
                    control<T, Allocator>(ptr, alloc)) {}

            template<typename U, typename D, typename Allocator>
            shared_ptr(U *ptr, D d, Allocator alloc):
                ptr(ptr),
                ctl(new (alloc.allocate(sizeof(control<T, Allocator, D>)))
                    control<T, Allocator, D>(ptr, d, alloc)) {}

            template<typename U>
            shared_ptr(const shared_ptr<U>& other, T *ptr) noexcept:
                ptr(ptr), ctl(other.ctl) {
                if (ctl)
                    ctl->increment_use();
            }

            shared_ptr(const shared_ptr& other) noexcept:
                ptr(other.ptr), ctl(other.ctl) {
                if (ctl)
                    ctl->increment_use();
            }

            template<typename U>
            shared_ptr(const shared_ptr<U>& other) noexcept:
                ptr(other.ptr), ctl(other.ctl) {
                if (ctl)
                    ctl->increment_use();
            }

            shared_ptr(shared_ptr&& other) noexcept:
                ptr(std::move(other.ptr)), ctl(std::move(other.ctl)) {
                other.ptr = nullptr;
                other.ctl = nullptr;
            }

            template<typename U>
            shared_ptr(shared_ptr<U>&& other) noexcept:
                ptr(std::move(other.ptr)), ctl(std::move(other.ctl)) {
                other.ptr = nullptr;
                other.ctl = nullptr;
            }

            template<typename U>
            explicit shared_ptr(const weak_ptr<U>& weak):
                ptr(weak.ptr), ctl(weak.ctl) {
                if (weak.expired()) {
                    __builtin_unreachable();
                } else {
                    ctl->increment_use();
                }
            }

            template<typename U, typename D>
            shared_ptr(unique_ptr<U, D>&& other):
                shared_ptr{other.release(), other.alloc, other.deleter} {}

            ~shared_ptr() {
                if (ctl)
                    ctl->decrement_use();
            }

            shared_ptr& operator=(const shared_ptr& rhs) noexcept {
                shared_ptr{rhs}.swap(*this);
                return *this;
            }

            template<typename U>
            shared_ptr& operator=(const shared_ptr<U>& rhs) noexcept {
                shared_ptr{rhs}.swap(*this);
                return *this;
            }

            shared_ptr& operator=(shared_ptr&& rhs) noexcept {
                shared_ptr{std::move(rhs)}.swap(*this);
                return *this;
            }

            template<typename U>
            shared_ptr& operator=(shared_ptr<U>&& rhs) noexcept {
                shared_ptr{std::move(rhs)}.swap(*this);
                return *this;
            }

            template<typename U, typename D>
            shared_ptr& operator=(unique_ptr<U, D>&& other) noexcept {
                shared_ptr{std::move(other)}.swap(*this);
                return *this;
            }

            void swap(shared_ptr& other) noexcept {
                std::swap(ptr, other.ptr);
                std::swap(ctl, other.ctl);
            }

            void reset() noexcept {
                shared_ptr{}.swap(*this);
            }

            template<typename U, typename Allocator>
            void reset(U *ptr, Allocator alloc) {
                shared_ptr{ptr, alloc}.swap(*this);
            }

            template<typename U, typename D, typename Allocator>
            void reset(U *ptr, D d, Allocator alloc) {
                shared_ptr{ptr, d, alloc}.swap(*this);
            }

            T *get() const {
                return ptr;
            }

            long use_count() const noexcept {
                if (ctl)
                    return ctl->use_count();

                return 0;
            }

            explicit operator bool() const {
                return (this->ptr != nullptr);
            }


            explicit operator void *() const {
                return this->ptr;
            }

            T *operator->() const {
                return this->ptr;
            }

            T& operator*() const {
                return *this->ptr;
            }

            template<typename U>
            bool owner_before(shared_ptr<U> const& other) const {
                return ctl < other.ctl;
            }

            template<typename U>
            bool owner_before(weak_ptr<U> const& other) const {
                return ctl < other.ctl;
            }
    };

    // operator ==
    template<typename T, typename U>
    inline bool operator==(const shared_ptr<T>& lhs,
        const shared_ptr<U>& rhs) {
        return lhs.get() == rhs.get();
    }

    template<typename T>
    inline bool operator==(const shared_ptr<T>& lhs, std::nullptr_t) noexcept {
        return !lhs;
    }

    template<typename T>
    inline bool operator==(std::nullptr_t, const shared_ptr<T>& rhs) noexcept {
        return !rhs;
    }

    // operator !=
    template<typename T, typename U>
    inline bool operator!=(const shared_ptr<T>& lhs,
        const shared_ptr<U>& rhs) {
        return lhs.get() != rhs.get();
    }

    template<typename T>
    inline bool operator!=(const shared_ptr<T>& lhs, std::nullptr_t) noexcept {
        return bool{lhs};
    }

    template<typename T>
    inline bool operator!=(std::nullptr_t, const shared_ptr<T>& rhs) noexcept {
        return bool{rhs};
    }

    // operator <
    template<typename T, typename U>
    inline bool operator<(const shared_ptr<T>& lhs,
        const shared_ptr<U>& rhs) {
        using T_element_type = typename shared_ptr<T>::element_type;
        using U_element_type = typename shared_ptr<U>::element_type;

        using common_type = typename std::common_type<T_element_type *, U_element_type *>::type;
        return ((common_type) lhs.get()) < ((common_type) rhs.get());
    }

    template<typename T>
    inline bool operator<(const shared_ptr<T>& lhs, std::nullptr_t) {
        using T_element_type = typename shared_ptr<T>::element_type;
        return ((T_element_type *) lhs.get()) < ((T_element_type *) nullptr);
    }

    template<typename T>
    inline bool operator<(std::nullptr_t, const shared_ptr<T>& rhs) {
        using T_element_type = typename shared_ptr<T>::element_type;
        return ((T_element_type *) nullptr) < ((T_element_type *) rhs.get());
    }

    // operator <=
    template<typename T, typename U>
    inline bool operator<=(const shared_ptr<T>& lhs,
        const shared_ptr<U>& rhs) {
        return !(rhs.get() < lhs.get());
    }

    template<typename T, typename U>
    inline bool operator<=(const shared_ptr<T>& lhs, std::nullptr_t) {
        return !(nullptr < lhs.get());
    }

    template<typename T, typename U>
    inline bool operator<=(std::nullptr_t, const shared_ptr<T>& rhs) {
        return !(rhs.get() < nullptr);
    }

    // operator >
    template<typename T, typename U>
    inline bool operator>(const shared_ptr<T>& lhs,
        const shared_ptr<U>& rhs) {
        return rhs.get() < lhs.get();
    }

    template<typename T, typename U>
    inline bool operator>(const shared_ptr<T>& lhs, std::nullptr_t) {
        return nullptr < lhs.get();
    }

    template<typename T, typename U>
    inline bool operator>(std::nullptr_t, const shared_ptr<T>& rhs) {
        return rhs.get() < nullptr;
    }

    // operator >=
    template<typename T, typename U>
    inline bool operator>=(const shared_ptr<T>& lhs,
        const shared_ptr<U>& rhs) {
        return !(lhs.get() > rhs.get());
    }

    template<typename T, typename U>
    inline bool operator>=(const shared_ptr<T>& lhs, std::nullptr_t) {
        return !(lhs.get() < nullptr);
    }

    template<typename T, typename U>
    inline bool operator>=(std::nullptr_t, const shared_ptr<T>& rhs) {
        return !(nullptr < rhs.get());
    }

    // swap
    template<typename T>
    inline void swap(shared_ptr<T>& lhs, shared_ptr<T>& rhs) {
        lhs.swap(rhs);
    }

    // creation and casting

    template<typename T, typename Allocator, typename... Args>
    shared_ptr<T> allocate_shared(Allocator alloc, Args &&... args) {
        auto memory = alloc.allocate(sizeof(T));
        return shared_ptr<T>{new (memory) T(std::forward<Args>(args)...), alloc};
    }

    template<typename T, typename U>
    inline shared_ptr<T> reinterpret_pointer_cast(const shared_ptr<U>& other) noexcept {
        using p = shared_ptr<T>;
        return p(other, reinterpret_cast<typename p::element_type *>(other.get()));
    }

    template<typename T>
    struct weak_ptr {
        private:
            T *ptr;
            control_base *ctl;
        public:
            template<typename U>
            friend struct shared_ptr;

            template<typename U>
            friend struct weak_ptr;

            using element_type = typename std::remove_extent<T>::type;

            constexpr weak_ptr() noexcept:
                ptr(), ctl() {}

            template<typename U>
            weak_ptr(shared_ptr<U> const& other) noexcept:
                ptr(other.ptr), ctl(other.ctl) {
                if (ctl)
                    ctl->increment_weak();
            }

            weak_ptr(weak_ptr const& other) noexcept:
                ptr(other.ptr), ctl(other.ctl) {
                if (ctl)
                    ctl->increment_weak();
            }

            template<typename U>
            weak_ptr(weak_ptr<U> const& other) noexcept:
                ptr(other.ptr), ctl(other.ctl) {
                if (ctl)
                    ctl->increment_weak();
            }

            ~weak_ptr() {
                if (ctl)
                    ctl->decrement_weak();
            }

            void swap(weak_ptr& other) noexcept {
                std::swap(ptr, other.ptr);
                std::swap(ctl, other.ctl);
            }

            weak_ptr& operator=(const weak_ptr& other) noexcept {
                weak_ptr{other}.swap(*this);
                return *this;
            }

            template<typename U>
            weak_ptr& operator=(const weak_ptr<U>& other) noexcept {
                weak_ptr{other}.swap(*this);
                return *this;
            }

            template<typename U>
            weak_ptr& operator=(const shared_ptr<U>& other) noexcept {
                weak_ptr{other}.swap(*this);
                return *this;
            }

            void reset() noexcept {
                weak_ptr{}.swap(*this);
            }

            long use_count() const noexcept {
                if (ctl)
                    return ctl->use_count();

                return 0;
            }

            bool expired() const noexcept {
                if (ctl)
                    return ctl->expired();

                return false;
            }

            shared_ptr<T> lock() const noexcept {
                return (expired() ? shared_ptr<T>{} : shared_ptr<T>{*this});
            }

            template<typename U>
            bool owner_before(shared_ptr<U> const& other) const {
                return ctl < other.ctl;
            }

            template<typename U>
            bool owner_before(weak_ptr<U> const& other) const {
                return ctl < other.ctl;
            }            
    };

    template<typename T>
    inline void swap(weak_ptr<T>& lhs, weak_ptr<T>& rhs) {
        lhs.swap(rhs);
    }
}

#endif