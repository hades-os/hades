#ifndef PRS_UNIQUE_HPP
#define PRS_UNIQUE_HPP

#include <cstddef>
#include <type_traits>
#include <utility>

#include "prs/assert.hpp"
#include "prs/deleter.hpp"

namespace prs {
    template<typename T> struct shared_ptr;

    template<typename T, typename Allocator, typename D = default_deleter<T, Allocator>>
    struct unique_ptr {
        private:
            T *ptr;
            D _deleter;
        public:
            template<typename U>
            friend struct shared_ptr;

            using pointer = T *;
            using element_type = T;
            using deleter_type = D;

            constexpr unique_ptr() noexcept = default;

            constexpr unique_ptr(std::nullptr_t) noexcept
                {}

            explicit unique_ptr(pointer p, Allocator alloc) noexcept:
                ptr(p), _deleter(alloc) {}

            unique_ptr(pointer p,
                typename std::conditional<std::is_reference<deleter_type>::value,
                    deleter_type, const deleter_type&>::type d) noexcept:
                ptr(p), _deleter(d) {}

            unique_ptr(pointer p,
                typename std::remove_reference<deleter_type>::type&& d) noexcept:
                ptr(p), _deleter(std::move(d)) {}

            unique_ptr(unique_ptr&& other) noexcept:
                ptr(other.release()), _deleter(other.deleter()) {}

            template<typename U, typename E>
            unique_ptr(unique_ptr<U, E>&& other) noexcept:
                ptr(other.release()), _deleter(other.deleter()) {}

            ~unique_ptr() noexcept {
                if(ptr)
                    _deleter(ptr);
            }

            unique_ptr& operator=(unique_ptr&& other) noexcept {
                reset(other.release());
                auto& _deleter = this->deleter();
                _deleter = other.deleter();

                return *this;
            }

            template<typename U, typename E>
            unique_ptr& operator=(unique_ptr<U, E>&& other) noexcept {
                reset(other.release());
                auto& _deleter = this->deleter();
                _deleter = other.deleter();

                return *this;
            }

            unique_ptr& operator=(std::nullptr_t) noexcept {
                reset();
                return *this;
            }

            element_type& operator *() const noexcept {
                assert(ptr);
                return *ptr;
            }

            pointer operator->() const noexcept {
                assert(ptr);
                return ptr;
            }

            pointer get() const noexcept {
                return ptr;
            }

            deleter_type& deleter() noexcept {
                return _deleter;
            }

            const deleter_type& deleter() const noexcept {
                return _deleter;
            }

            explicit operator bool() const noexcept {
                return (ptr != nullptr);
            }

            pointer release() noexcept {
                pointer cp = ptr;
                ptr = nullptr;

                return cp;
            }

            void reset(pointer p) noexcept {
                auto& _ptr = ptr;
                auto& _deleter = deleter();
                if (ptr)
                    _deleter(ptr);

                ptr = p;
            }

            void reset() noexcept {
                auto& _ptr = ptr;
                auto& _deleter = deleter();
                if (ptr)
                    _deleter(ptr);

                ptr = pointer{};
            }

            void swap(unique_ptr& other) noexcept {
                swap(ptr, other.ptr);
            }

            unique_ptr(const unique_ptr&) = delete;
            unique_ptr& operator=(const unique_ptr&) = delete;
        };

        template<typename T, typename Allocator, typename... Args>
        unique_ptr<T, Allocator> make_unique(Allocator alloc, Args&&... args) {
            auto memory = alloc.allocate(sizeof(T));
            return unique_ptr<T, Allocator>{new (memory) T{std::forward<Args>(args)...}, alloc};
        }

    // operator ==
    template<typename T, typename D,
        typename U, typename E>
    inline bool operator==(const unique_ptr<T, D>& lhs,
        const unique_ptr<U, E>& rhs) {
        return lhs.get() == rhs.get();
    }

    template<typename T, typename D>
    inline bool operator==(const unique_ptr<T, D>& lhs, std::nullptr_t) noexcept {
        return !lhs;
    }

    template<typename T, typename D>
    inline bool operator==(std::nullptr_t, const unique_ptr<T, D>& rhs) noexcept {
        return !rhs;
    }

    // operator !=
    template<typename T, typename D,
        typename U, typename E>
    inline bool operator!=(const unique_ptr<T, D>& lhs,
        const unique_ptr<U, E>& rhs) {
        return lhs.get() != rhs.get();
    }

    template<typename T, typename D>
    inline bool operator!=(const unique_ptr<T, D>& lhs, std::nullptr_t) noexcept {
        return bool{lhs};
    }

    template<typename T, typename D>
    inline bool operator!=(std::nullptr_t, const unique_ptr<T, D>& rhs) noexcept {
        return bool{rhs};
    }

    // operator <
    template<typename T, typename D,
        typename U, typename E>
    inline bool operator<(const unique_ptr<T, D>& lhs,
        const unique_ptr<U, E>& rhs) {
        using T_element_type = typename unique_ptr<T, D>::element_type;
        using U_element_type = typename unique_ptr<U, E>::element_type;

        using common_type = typename std::common_type<T_element_type *, U_element_type *>::type;
        return ((common_type) lhs.get()) < ((common_type) rhs.get());
    }

    template<typename T,typename D>
    inline bool operator<(const unique_ptr<T, D>& lhs, std::nullptr_t) {
        using T_element_type = typename unique_ptr<T, D>::element_type;
        return ((T_element_type *) lhs.get()) < ((T_element_type *) nullptr);
    }

    template<typename T,typename D>
    inline bool operator<(std::nullptr_t, const unique_ptr<T, D>& rhs) {
        using T_element_type = typename unique_ptr<T, D>::element_type;
        return ((T_element_type *) nullptr) < ((T_element_type *) rhs.get());
    }

    // operator <=
    template<typename T, typename D,
        typename U, typename E>
    inline bool operator<=(const unique_ptr<T, D>& lhs,
        const unique_ptr<U, E>& rhs) {
        return !(rhs.get() < lhs.get());
    }

    template<typename T, typename D>
    inline bool operator<=(const unique_ptr<T, D>& lhs, std::nullptr_t) {
        return !(nullptr < lhs.get());
    }

    template<typename T, typename D>
    inline bool operator<=(std::nullptr_t, const unique_ptr<T, D>& rhs) {
        return !(rhs.get() < nullptr);
    }

    // operator >
    template<typename T, typename D,
        typename U, typename E>
    inline bool operator>(const unique_ptr<T, D>& lhs,
        const unique_ptr<U, E>& rhs) {
        return rhs.get() < lhs.get();
    }

    template<typename T, typename D>
    inline bool operator>(const unique_ptr<T, D>& lhs, std::nullptr_t) {
        return nullptr < lhs.get();
    }

    template<typename T, typename D>
    inline bool operator>(std::nullptr_t, const unique_ptr<T, D>& rhs) {
        return rhs.get() < nullptr;
    }

    // operator >=
    template<typename T, typename D,
        typename U, typename E>
    inline bool operator>=(const unique_ptr<T, D>& lhs,
        const unique_ptr<U, E>& rhs) {
        return !(lhs.get() > rhs.get());
    }

    template<typename T, typename D>
    inline bool operator>=(const unique_ptr<T, D>& lhs, std::nullptr_t) {
        return !(lhs.get() < nullptr);
    }

    template<typename T, typename D>
    inline bool operator>=(std::nullptr_t, const unique_ptr<T, D>& rhs) {
        return !(nullptr < rhs.get());
    }

    template<typename T, typename D>
    inline void swap(unique_ptr<T, D>& lhs, unique_ptr<T, D>& rhs) {
        lhs.swap(rhs);
    }
}

#endif