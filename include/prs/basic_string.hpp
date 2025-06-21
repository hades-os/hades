#ifndef PRS_BASIC_STRING_HPP
#define PRS_BASIC_STRING_HPP

#include <algorithm>
#include <compare>
#include <cstddef>

#include "prs/allocator.hpp"
#include "prs/assert.hpp"
#include "util/string.hpp"

namespace prs {
    template<typename Char, typename Allocator>
    struct basic_string;

    template<typename Char>
    struct basic_string_view {
        private:
            const Char *_p;
            size_t _length;
        public:
            template<typename X, typename Allocator>
            friend struct basic_string;

            struct iterator {
                private:
                    Char *current;
                public:
                    using value_type = Char;
                    using pointer = Char *;
                    using reference = Char;
                    using difference_type = std::ptrdiff_t;

                    iterator(pointer ptr):
                        current(ptr) {}

                    reference operator *() const {
                        return *current;
                    }

                    pointer operator->() {
                        return current;
                    }

                    iterator& operator++() {
                        current++;
                        return *this;
                    }

                    iterator operator++(int) {
                        auto copy = *this;
                        ++(*this);
                        return copy;
                    }

                    explicit operator bool() {
                        return current != nullptr;
                    }

                    friend bool operator==(const iterator& a, const iterator& b) {
                        return a.current == b.current;
                    }

                    friend bool operator!=(const iterator& a, const iterator& b) {
                        return a.current != b.current;
                    }

                    friend iterator operator+(size_t n, const iterator& a) {
                        return iterator{a.current + n};
                    }

                    friend iterator operator-(size_t n, const iterator& a) {
                        return iterator{a.current - n};
                    }

                    friend difference_type operator-(const iterator& a, const iterator& b) {
                        return a.current - b.current;
                    }
            };

            struct reverse_iterator {
                private:
                    Char *current;
                public:
                    using value_type = Char;
                    using pointer = Char *;
                    using reference = Char;
                    using difference_type = std::ptrdiff_t;

                    reverse_iterator(pointer ptr):
                        current(ptr) {}

                    reference operator *() const {
                        return *current;
                    }

                    pointer operator->() {
                        return current;
                    }

                    reverse_iterator& operator++() {
                        current--;
                        return *this;
                    }

                    reverse_iterator operator++(int) {
                        auto copy = *this;
                        ++(*this);
                        return copy;
                    }

                    explicit operator bool() {
                        return current != nullptr;
                    }

                    friend bool operator==(const reverse_iterator& a,
                            const reverse_iterator& b) {
                        return a.current == b.current;
                    }

                    friend bool operator!=(const reverse_iterator& a,
                            const reverse_iterator& b) {
                        return a.current != b.current;
                    }

                    friend reverse_iterator operator+(size_t n, const reverse_iterator& a) {
                        return iterator{a.current + n};
                    }

                    friend reverse_iterator operator-(size_t n, const reverse_iterator& a) {
                        return reverse_iterator{a.current - n};
                    }

                    friend difference_type operator-(const reverse_iterator& a, const reverse_iterator& b) {
                        return a.current - b.current;
                    }
            };

            basic_string_view():
                _p(nullptr), _length(0) {}

            basic_string_view(const Char *cs):
                _p(cs), _length(0) {
                while(cs[_length])
                    _length++;
            }

            basic_string_view(const Char *cs, size_t length):
                _p(cs), _length(length) {}


            iterator begin() {
                return iterator{_p};
            }

            iterator end() {
                return iterator{_p + _length};
            }

            reverse_iterator rbegin() {
                return reverse_iterator{_p + _length};
            }

            reverse_iterator rend() {
                return reverse_iterator{_p};
            }

            const Char& operator[] (size_t index) {
                prs::assert(index < _length);
                return _p[index];
            }

            bool operator== (basic_string_view other) const {
                if(_length != other._length)
                    return false;
                for(size_t i = 0; i < _length; i++)
                    if(_p[i] != other._p[i])
                        return false;
                return true;
            }

            bool operator== (const Char *s) const {
                basic_string_view other{s};
                if(_length != other._length)
                    return false;
                for(size_t i = 0; i < _length; i++)
                    if(_p[i] != other._p[i])
                        return false;
                return true;
            }

            bool operator== (const char c) const {
                if(_length != 1)
                    return false;
                return _p[0] == c;
            }

            std::strong_ordering operator<=>(basic_string_view other) const {
                size_t rlen = std::min(_length, other._length);
                for (size_t i = 0; i < rlen; i++) {
                    if (_p[i] < other._p[i]) return std::strong_ordering::less;
                    if (_p[i] > other._p[i]) return std::strong_ordering::greater;
                }

                if (_length < other._length) return std::strong_ordering::less;
                if (_length > other._length) return std::strong_ordering::greater;

                return std::strong_ordering::equal;
            }

            std::strong_ordering operator<=>(const Char *s) const {
                basic_string_view other{s};

                size_t rlen = std::min(_length, other._length);
                for (size_t i = 0; i < rlen; i++) {
                    if (_p[i] < other._p[i]) return std::strong_ordering::less;
                    if (_p[i] > other._p[i]) return std::strong_ordering::greater;
                }

                if (_length < other._length) return std::strong_ordering::less;
                if (_length > other._length) return std::strong_ordering::greater;

                return std::strong_ordering::equal;
            }

            const Char& front() {
                return _p[0];
            }

            const Char& back() {
                return _p[_length];
            }

            const Char *data() const {
                return _p;
            }

            size_t size() const {
                return _length;
            }

            bool empty() {
                return _length == 0;
            }

            void remove_prefix(size_t n) {
                prs::assert(n < _length);
                _p += n;
                _length -= n;
            }

            void remove_suffix(size_t n) {
                prs::assert(n < _length);
                _length -= n;
            }

            size_t copy(Char *dest, size_t count, size_t pos = 0) {
                prs::assert(pos < _length);

                size_t rcount = std::min(count, _length - pos);
                memcpy(dest, _p + pos, rcount);

                return rcount;
            }

            basic_string_view substring(size_t pos, size_t count) {
                prs::assert(pos + count <= _length);
                return basic_string_view{_p + pos, count};
            }

            basic_string_view substring(size_t pos) {
                size_t size = _length - pos;

                prs::assert(pos + size <= _length);
                return basic_string_view{_p + pos, size};
            }

            int compare(basic_string_view other) {
                size_t rlen = std::min(_length, other._length);
                return strncmp(_p, other._p, rlen);
            }

            int compare(size_t pos, size_t count, basic_string_view other) {
                return substring(pos, count).compare(other);
            }

            int compare(size_t pos, size_t count,
                basic_string_view other, size_t other_pos, size_t other_count) {
                return substring(pos, count).compare(other.substring(other_pos, other_count));
            }

            int compare(const Char *s) {
                return basic_string_view{s}.compare(this);
            }

            int compare(size_t pos, size_t count, const Char *s) {
                return substring(pos, count).compare(basic_string_view{s});
            }

            int compare(size_t pos, size_t count,
                const Char *s, size_t s_pos, size_t s_count) {
                return substring(pos, count).compare(basic_string_view{s}.substring(s_pos, s_count));
            }

            bool startswith(basic_string_view other) const {
                if (_length < other._length)
                    return false;

                for (size_t i = 0; i < other._length; i++) {
                    if (_p[i] != other._p[i])
                        return false;
                }

                return true;
            }

            bool startswith(Char c) const {
                if (_p[0] == c)
                    return true;

                return false;
            }

            bool startswith(const Char *s) const {
                return startswith(basic_string_view{s});
            }

            bool endswith(basic_string_view other) const {
                if (_length < other._length)
                    return false;

                for (size_t i = 0; i < other._length; i++) {
                    if (_p[_length - other._length + i] != other[i])
                        return false;
                }

                return true;
            }

            bool endswith(Char c) const {
                if (_p[_length] == c)
                    return true;

                return false;
            }

            bool endswith(const Char *s) const {
                return endswith(basic_string_view{s});
            }

            bool contains(basic_string_view other) const {
                if (_length < other._length)
                    return false;

                for (size_t i = 0; i < _length - other._length; i++) {
                    bool match = true;
                    for (size_t j = 0; i < other._length; j++) {
                        if (_p[i + j] != other._p[j]) {
                            match = false;
                            break;
                        }
                    }

                    if (match)
                        return true;
                }

                return false;
            }

            bool contains(Char c) const {
                for (size_t i = 0; i < _length; i++) {
                    if (_p[i] == c)
                        return true;
                }

                return false;
            }

            bool contains(const Char *s) const {
                return contains(basic_string_view{s});
            }

            size_t find(basic_string_view other, size_t pos = 0) const {
                if (_length < other._length)
                    return size_t(-1);

                for (size_t i = pos; i < _length - other._length; i++) {
                    bool match = true;
                    for (size_t j = 0; i < other._length; j++) {
                        if (_p[i + j] != other._p[j]) {
                            match = false;
                            break;
                        }
                    }

                    if (match)
                        return i;
                }

                return size_t(-1);
            }

            size_t find(Char c, size_t pos = 0) const {
                return find(basic_string_view{&c, 1}, pos);
            }

            size_t find(const Char *s, size_t pos, size_t count) const {
                return find(basic_string_view{s, count}, pos);
            }

            size_t find(const Char *s, size_t pos = 0) const {
                return find(basic_string_view{s}, pos);
            }

            size_t rfind(basic_string_view other, size_t pos = size_t(-1)) const {
                if (_length < other._length)
                    return size_t(-1);

                if (pos == size_t(-1))
                    pos = _length - other._length;

                for (size_t i = pos; i >= 0; i--) {
                    bool match = true;
                    for (size_t j = 0; i < other._length; j++) {
                        if (_p[i + j] != other._p[j]) {
                            match = false;
                            break;
                        }
                    }

                    if (match)
                        return i;
                }

                return size_t(-1);
            }

            size_t rfind(Char c, size_t pos = size_t(-1)) const {
                return rfind(basic_string_view{&c, 1}, pos);
            }

            size_t rfind(const Char *s, size_t pos, size_t count) const {
                return rfind(basic_string_view{s, count}, pos);
            }

            size_t rfind(const Char *s, size_t pos = 0) const {
                return rfind(basic_string_view{s}, pos);
            }

            size_t find_first(Char c, size_t pos = 0) {
                for(size_t i = pos; i < _length; i++)
                    if(_p[i] == c)
                        return i;

                return size_t(-1);
            }

            size_t find_last(Char c) {
                for(size_t i = _length; i > 0; i--)
                    if(_p[i - 1] == c)
                        return i - 1;

                return size_t(-1);
            }

            size_t count(Char c) const {
                size_t count = 0;
                for (size_t i = 0; i < _length; i++) {
                    if (_p[i] == c) {
                        count++;
                    }
                }

                return count;
            }
    };

    constexpr size_t sso_capacity = 16;
    constexpr size_t sso_size = sso_capacity - 1;

    template<typename Char, typename Allocator>
    struct basic_string {
        private:
            union {
                size_t capacity;
                Char data[sso_capacity];
            } _sso;

            Char *_p;
            size_t _length;
            Allocator allocator = Allocator();

            void init(size_t length) {
                _length = length;
                if (length > sso_size) {
                    size_t capacity = length + 1;

                    _sso.capacity = capacity;
                    _sso.data[sso_size] = '\1';

                    _p = (Char *) allocator.allocate(capacity);
                } else {
                    _sso.data[sso_size] = '\0';
                    _p = _sso.data;
                }
            }

            void ensure_capacity(size_t new_capacity) {
                if (_sso.data[sso_size] == '\1') {
                    if (new_capacity > _sso.capacity) {
                        _sso.capacity = new_capacity;
                        _p = (Char *) allocator.reallocate(_p, new_capacity);
                    }
                } else {
                    if (new_capacity > sso_size) {
                        Char *new_data = (Char *) allocator.allocate(new_capacity);

                        memcpy(new_data, _p, _length + 1);

                        _p = new_data;
                        _sso.data[sso_size] = '\1';
                        _sso.capacity = new_capacity;
                    }
                }
            }

            basic_string(size_t length, size_t capacity) {
                _length = length;
                if (capacity > sso_capacity) {
                    _p = (Char *) allocator.allocate(capacity);
                    _sso.capacity = capacity;
                    _sso.data[sso_size] = '\1';
                } else {
                    _p = _sso.data;
                    _sso.data[sso_size] = '\0';
                }
            }

        public:
            struct iterator {
                private:
                    Char *current;
                public:
                    using value_type = Char;
                    using pointer = Char *;
                    using reference = Char;
                    using difference_type = std::ptrdiff_t;

                    iterator(pointer ptr):
                        current(ptr) {}

                    reference operator *() const {
                        return *current;
                    }

                    pointer operator->() {
                        return current;
                    }

                    iterator& operator++() {
                        current++;
                        return *this;
                    }

                    iterator operator++(int) {
                        auto copy = *this;
                        ++(*this);
                        return copy;
                    }

                    explicit operator bool() {
                        return current != nullptr;
                    }

                    friend bool operator==(const iterator& a, const iterator& b) {
                        return a.current == b.current;
                    }

                    friend bool operator!=(const iterator& a, const iterator& b) {
                        return a.current != b.current;
                    }

                    friend iterator operator+(size_t n, const iterator& a) {
                        return iterator{a.current + n};
                    }

                    friend iterator operator-(size_t n, const iterator& a) {
                        return iterator{a.current - n};
                    }

                    friend difference_type operator-(const iterator& a, const iterator& b) {
                        return a.current - b.current;
                    }
            };

            struct reverse_iterator {
                private:
                    Char *current;
                public:
                    using value_type = Char;
                    using pointer = Char *;
                    using reference = Char;
                    using difference_type = std::ptrdiff_t;

                    reverse_iterator(pointer ptr):
                        current(ptr) {}

                    reference operator *() const {
                        return *current;
                    }

                    pointer operator->() {
                        return current;
                    }

                    reverse_iterator& operator++() {
                        current--;
                        return *this;
                    }

                    reverse_iterator operator++(int) {
                        auto copy = *this;
                        ++(*this);
                        return copy;
                    }

                    explicit operator bool() {
                        return current != nullptr;
                    }

                    friend bool operator==(const reverse_iterator& a,
                            const reverse_iterator& b) {
                        return a.current == b.current;
                    }

                    friend bool operator!=(const reverse_iterator& a,
                            const reverse_iterator& b) {
                        return a.current != b.current;
                    }

                    friend reverse_iterator operator+(size_t n, const reverse_iterator& a) {
                        return iterator{a.current + n};
                    }

                    friend reverse_iterator operator-(size_t n, const reverse_iterator& a) {
                        return reverse_iterator{a.current - n};
                    }

                    friend difference_type operator-(const reverse_iterator& a, const reverse_iterator& b) {
                        return a.current - b.current;
                    }
            };
        
            basic_string():
                _p(_sso.data), _length(0) {
                _sso.data[0] = '\0';
                _sso.data[sso_size] = '\0';
            }

            basic_string(decltype(nullptr)):
                _p(_sso.data), _length(0) {
                _sso.data[0] = '\0';
                _sso.data[sso_size] = '\0';
            }

            basic_string(const Char *value) {
                if (value == nullptr) {
                    _length = 0;
                    _sso.data[0] = '\0';
                    _sso.data[15] ='\0';
                } else {
                    size_t length;
                    while (value[length])
                        length++;

                    init(length);
                    memcpy(_p, value, _length);
                }
            }

            basic_string(const Char *value, size_t length) {
                init(length);
                memcpy(_p, value, _length);
            }

            explicit basic_string(Char c) {
                init(1);
                memcpy(_p, c, _length);
            }

            ~basic_string() {
                if (_sso.data[sso_size] == '\1')
                    allocator.free(_p);
            }

            size_t size() {
                return _length;
            }

            basic_string(basic_string const& other) {
                if (other._sso.data[sso_size] == '\1') {
                    _length = other._length;
                    _sso.capacity = _length + 1;
                    _sso.data[sso_size] = '\1';

                    _p = (Char *) allocator.allocate(_sso.capacity);
                    memcpy(_p, other._p, _sso.capacity);
                } else {
                    _p = _sso.data;
                    _length = other._length;
                    memcpy(&_sso.data, &other._sso.data, _length + 1);
                    _sso.data[sso_size] = '\0';
                }
            }

            basic_string(basic_string_view<Char> const& other) {
                _length = other._length;
                _sso.capacity = _length + 1;
                _sso.data[sso_size] = '\1';

                _p = (Char *) allocator.allocate(_sso.capacity);
                memcpy(_p, other._p, _sso.capacity);
            }

            basic_string& operator=(basic_string const& other) {
                if (other._sso.data[sso_size] == '\1') {
                    _length = other._length;
                    _sso.capacity = _length + 1;
                    _sso.data[sso_size] = '\1';

                    _p = (Char *) allocator.allocate(_sso.capacity);
                    memcpy(_p, other._p, _sso.capacity);
                } else {
                    _p = _sso.data;
                    _length = other._length;
                    memcpy(&_sso.data, &other._sso.data, _length + 1);
                    _sso.data[sso_size] = '\0';
                }

                return *this;
            }

            basic_string& operator=(basic_string_view<Char> const& other) {
                _length = other._length;
                _sso.capacity = _length + 1;
                _sso.data[sso_size] = '\1';

                _p = (Char *) allocator.allocate(_sso.capacity);
                memcpy(_p, other._p, _sso.capacity);

                return *this;
            }

            basic_string(basic_string&& other) noexcept {
                if (other._sso.data[sso_size] == '\1') {
                    _length = other._length;
                    _sso.capacity = other._sso.capacity;
                    _sso.data[sso_size] = '\1';

                    _p = other._p;
                    other._sso.data[sso_size] = '\0';
                } else {
                    _p = _sso.data;
                    _length = other._length;
                    memcpy(&_sso.data, &other._sso.data, _length + 1);
                    _sso.data[sso_size] = '\0';
                }
            }

            basic_string& operator=(basic_string&& other) noexcept {
                if (this != &other) {
                    if (other._sso.data[sso_size] == '\1') {
                        _length = other._length;
                        _sso.capacity = other._sso.capacity;
                        _sso.data[sso_size] = '\1';

                        _p = other._p;
                        other._sso.data[sso_size] = '\0';
                    } else {
                        _p = _sso.data;
                        _length = other._length;
                        memcpy(&_sso.data, &other._sso.data, _length + 1);
                        _sso.data[sso_size] = '\0';
                    }
                }

                return *this;
            }

            const Char *data() {
                return _p;
            }

            bool empty() const {
                return _length == 0;
            }

            operator const char *() {
                return _p;
            }

            iterator begin() {
                return iterator{_p};
            }

            iterator end() {
                return iterator{_p + _length};
            }

            reverse_iterator rbegin() {
                return reverse_iterator{_p + _length};
            }

            reverse_iterator rend() {
                return reverse_iterator{_p};
            }

            Char &operator[](size_t index) {
                prs::assert(_p && index < _length);
                return _p[index];
            }

            bool operator !() {
                return _length == 0;
            }

            explicit operator bool() {
                return _length != 0;
            }

            bool operator==(const Char *value) {
                return strcmp(_p, value) == 0;
            }

            bool operator!=(const Char *value) {
                return strcmp(_p, value) != 0;
            }

            bool operator==(basic_string other) {
                return _length == other._length && strcmp(_p, other._p) == 0;
            }

            bool operator==(basic_string_view<Char> other) {
                return _length == other._length && strcmp(_p, other._p) == 0;
            }

            std::strong_ordering operator<=>(const Char *s) const {
                basic_string_view other{s};

                size_t rlen = std::min(_length, other._length);
                for (size_t i = 0; i < rlen; i++) {
                    if (_p[i] < other._p[i]) return std::strong_ordering::less;
                    if (_p[i] > other._p[i]) return std::strong_ordering::greater;
                }

                if (_length < other._length) return std::strong_ordering::less;
                if (_length > other._length) return std::strong_ordering::greater;

                return std::strong_ordering::equal;
            }

            void insert(size_t pos, const basic_string& other) {
                size_t rlen = _length + other._length;
                ensure_capacity(rlen + 1);

                memmove(&_p[pos + other._length], &_p[pos], _length - pos);
                memcpy(&_p[pos], other._p, other._length);

                _p[rlen] = '\0';
                _length = rlen;
            }

            void insert(size_t pos, const basic_string_view<Char>& other) {
                size_t rlen = _length + other._length;
                ensure_capacity(rlen + 1);

                memmove(&_p[pos + other._length], &_p[pos], _length - pos);
                memcpy(&_p[pos], other._p, other._length);

                _p[rlen] = '\0';
                _length = rlen;
            }

            void insert(size_t pos, const Char *value) {
                size_t value_len = 0;
                while (value[value_len])
                    value_len++;

                size_t rlen = _length + value_len;
                ensure_capacity(rlen + 1);

                memmove(&_p[pos + value_len], &_p[pos], _length - pos);
                memcpy(&_p[pos], value, value_len);

                _p[rlen] = '\0';
                _length = rlen;
            }

            void insert(size_t pos, Char value) {
                size_t rlen = _length + 1;
                ensure_capacity(rlen + 1);

                memmove(&_p[pos + 1], &_p[pos], _length - pos);
                memcpy(&_p[pos], &value, 1);

                _p[rlen] = '\0';
                _length = rlen;
            }            

            template<typename T>
            void insert(size_t pos, T const& value) {
                insert(pos, basic_string{value});
            }

            void append(basic_string const& other) {
                size_t rlen = _length + other._length;
                ensure_capacity(rlen + 1);

                memcpy(&_p, other._p, other._length);

                _p[rlen] = '\0';
                _length = rlen;
            }

            void append(basic_string_view<Char> const& other) {
                size_t rlen = _length + other._length;
                ensure_capacity(rlen + 1);

                memcpy(&_p, other._p, other._length);

                _p[rlen] = '\0';
                _length = rlen;
            }

            void append(const Char *value) {
                size_t value_len = 0;
                while (value[value_len])
                    value_len++;

                size_t rlen = _length + value_len;
                ensure_capacity(rlen + 1);

                memcpy(&_p, value, value_len);

                _p[rlen] = '\0';
                _length = rlen;
            }

            void append(Char value) {
                size_t rlen = _length + 1;
                ensure_capacity(rlen + 1);

                memcpy(&_p, &value, 1);

                _p[rlen] = '\0';
                _length = rlen;
            }            

            template<typename T>
            void append(T const& value) {
                append(basic_string{value});
            }

            basic_string& operator +=(basic_string const& other) {
                append(other);
                return *this;
            }

            basic_string& operator +=(basic_string_view<Char> value) {
                append(value);
                return *this;
            }

            basic_string& operator +=(const Char *value) {
                append(value);
                return *this;
            }

            basic_string& operator +=(Char value) {
                append(value);
                return *this;
            }            

            void erase(size_t pos, size_t count) {
                size_t rlen = _length - count;
                memmove(&_p[pos], &_p[pos + count], _length - pos - count);

                _p[rlen] = '\0';
                _length = rlen;
            }

            void remove(size_t pos) {
                erase(pos, 1);
            }

            basic_string friend operator +(basic_string const& lhs, basic_string const& rhs) {
                size_t rlen = lhs._length + rhs._length;
                basic_string s{rlen, rlen + 1};

                memcpy(s._p, lhs._p, lhs._length);
                memcpy(&s._p[lhs._length], rhs._p, rhs._length);

                s._p[rlen] = '\0';
                return s;
            }

            basic_string friend operator +(basic_string&& lhs, basic_string const& rhs) {
                size_t rlen = lhs._length + rhs._length;
                lhs.ensure_capacity(rlen + 1);

                memcpy(&lhs._p[lhs._length], rhs._p, rhs._length);

                lhs._length = rlen;
                lhs._p[rlen] = '\0';

                return std::move(lhs);
            }

            basic_string friend operator +(basic_string const& lhs, const Char *rhs) {
                size_t rhs_len = 0;
                while(rhs[rhs_len])
                    rhs_len++;

                size_t rlen = lhs._length + rhs_len;
                basic_string s{rlen, rlen + 1};

                memcpy(s._p, lhs._p, lhs._length);
                memcpy(&s._p[lhs._length], rhs, rhs_len);

                s._p[rlen] = '\0';
                return s;
            }

            basic_string friend operator +(const Char *lhs, basic_string const& rhs) {
                size_t lhs_len = 0;
                while(lhs[lhs_len])
                    lhs_len++;

                size_t rlen = rhs._length + lhs_len;
                basic_string s{rlen, rlen + 1};

                memcpy(s._p, rhs._p, rhs._length);
                memcpy(&s._p[rhs._length], lhs, lhs_len);

                s._p[rlen] = '\0';
                return s;
            }

            basic_string friend operator +(basic_string&& lhs, const Char *rhs) {
                size_t rhs_len = 0;
                while(rhs[rhs_len])
                    rhs_len++;

                size_t rlen = lhs._length + rhs_len;
                lhs.ensure_capacity(rlen);

                memcpy(&lhs._p[lhs._length], rhs, rhs_len);

                lhs._length = rlen;
                lhs._p[rlen] = '\0';

                return std::move(lhs);
            }

            basic_string friend operator +(basic_string_view<Char> const& lhs, basic_string const& rhs) {
                size_t rlen = lhs._length + rhs._length;
                basic_string s{rlen, rlen + 1};

                memcpy(s._p, lhs._p, lhs._length);
                memcpy(&s._p[lhs._length], rhs._p, rhs._length);

                s._p[rlen] = '\0';
                return s;
            }

            basic_string friend operator +(basic_string const& lhs, basic_string_view<Char> const& rhs) {
                size_t rlen = lhs._length + rhs._length;
                basic_string s{rlen, rlen + 1};

                memcpy(s._p, lhs._p, lhs._length);
                memcpy(&s._p[lhs._length], rhs._p, rhs._length);

                s._p[rlen] = '\0';
                return s;
            }

            basic_string friend operator +(basic_string&& lhs, basic_string_view<Char> const& rhs) {
                size_t rlen = lhs._length + rhs._length;
                lhs.ensure_capacity(rlen);

                memcpy(&lhs._p[lhs._length], rhs._p, rhs._length);

                lhs._length = rlen;
                lhs._p[rlen] = '\0';

                return std::move(lhs);
            }

            operator basic_string_view<Char>() const {
                return basic_string_view<Char>{_p, _length};
            }
        };
}

#endif