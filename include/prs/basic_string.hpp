#ifndef PRS_STRING_HPP
#define PRS_STRING_HPP

#include <algorithm>
#include <compare>
#include <cstddef>

#include "prs/assert.hpp"
#include "util/string.hpp"

namespace prs {
    template<typename Char>
    struct string_view {
        private:
            const Char *_p;
            size_t _length;
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

            string_view():
                _p(nullptr), _length(0) {}

            string_view(const Char *cs):
                _p(cs), _length(0) {
                while(cs[_length])
                    _length++;
            }

            string_view(const Char *cs, size_t length):
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

            bool operator== (string_view other) const {
                if(_length != other._length)
                    return false;
                for(size_t i = 0; i < _length; i++)
                    if(_p[i] != other._p[i])
                        return false;
                return true;
            }

            bool operator== (const Char *s) const {
                string_view other{s};
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

            std::strong_ordering operator<=>(string_view other) const {
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
                string_view other{s};

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

            string_view substring(size_t pos, size_t count) {
                prs::assert(pos + count <= _length);
                return string_view{_p + pos, count};
            }

            string_view substring(size_t pos) {
                size_t size = _length - pos;

                prs::assert(pos + size <= _length);
                return string_view{_p + pos, size};
            }

            int compare(string_view other) {
                size_t rlen = std::min(_length, other._length);
                return strncmp(_p, other._p, rlen);
            }

            int compare(size_t pos, size_t count, string_view other) {
                return substring(pos, count).compare(other);
            }

            int compare(size_t pos, size_t count,
                string_view other, size_t other_pos, size_t other_count) {
                return substring(pos, count).compare(other.substring(other_pos, other_count));
            }

            int compare(const Char *s) {
                return string_view{s}.compare(this);
            }

            int compare(size_t pos, size_t count, const Char *s) {
                return substring(pos, count).compare(string_view{s});
            }

            int compare(size_t pos, size_t count,
                const Char *s, size_t s_pos, size_t s_count) {
                return substring(pos, count).compare(string_view{s}.substring(s_pos, s_count));
            }

            bool startswith(string_view other) const {
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
                return startswith(string_view{s});
            }

            bool endswith(string_view other) const {
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
                return endswith(string_view{s});
            }

            bool contains(string_view other) const {
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
                return contains(string_view{s});
            }

            size_t find(string_view other, size_t pos = 0) const {
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
                return find(string_view{&c, 1}, pos);
            }

            size_t find(const Char *s, size_t pos, size_t count) const {
                return find(string_view{s, count}, pos);
            }

            size_t find(const Char *s, size_t pos = 0) const {
                return find(string_view{s}, pos);
            }

            size_t rfind(string_view other, size_t pos = size_t(-1)) const {
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
                return rfind(string_view{&c, 1}, pos);
            }

            size_t rfind(const Char *s, size_t pos, size_t count) const {
                return rfind(string_view{s, count}, pos);
            }

            size_t rfind(const Char *s, size_t pos = 0) const {
                return rfind(string_view{s}, pos);
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

    template<typename Char>
    struct string {
        
    }
}

#endif