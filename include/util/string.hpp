#ifndef STRING_HPP
#define STRING_HPP

#include <cstddef>
#include <cstdint>

extern "C" {
    inline void *memset(const void *dst, int val, size_t length) {
        char *dstptr = (char *) dst;
        for (size_t i = 0; i < length; i++) {
            dstptr[i] = val;
        }

        return (void *) dst;
    }

    inline void *memset64(const void *dst, int val, size_t length) {
        uint64_t *dstptr = (uint64_t *) dst;
        for (size_t i = 0; i < length / 8; i++) {
            dstptr[i] = val;
        }

        return (void *) dst;
    }

    inline void *memcpy(const void *dst, const void *src, size_t length) {
        char *dstptr = (char *) dst;
        char *srcptr = (char *) src;
        for (size_t i = 0; i < length; i++) {
            dstptr[i] = srcptr[i];
        }

        return (void *) dst;
    }

    inline size_t strncmp(const char *s1, const char *s2, size_t n) {
        while(n && *s1 && (*s1 == *s2)) {
            s1++;
            s2++;
            --n;
        }

        return !n ? 0 : *s1 - *s2;
    }

    inline int tolower(unsigned char ch) {
        if (ch >= 'A' && ch <= 'Z')
            ch = 'a' + (ch - 'A');
        return ch;
    }

    inline size_t strncasecmp(const char *s1, const char *s2, size_t n) {
        if (n != 0) {
            const unsigned char *us1 = (const uint8_t *) s1;
            const unsigned char *us2 = (const uint8_t *) s2;

            do {
                if (tolower(*us1) != tolower(*us2++))
                    return (tolower(*us1) - tolower(*--us2));
                if (*us1++ == '\0')
                    break;
            } while (--n != 0);
        }

        return 0;
    }

    inline size_t strcmp(const char *s1, const char *s2) {
        while(*s1 && (*s1 == *s2)) {
            s1++;
            s2++;
        }

        return *s1 - *s2;
    }

    inline size_t strlen(const char *s) {
        size_t len = 0;
        char *src = (char *) s;
        while (*src) {
            len++;
            src++;
        }

        return len;
    }

    inline char *strcpy(const char *dst, const char *src) {
        char *source = (char *) src;
        char *dest = (char *) dst;
        while ((*dest++ = *source++));
        
        return dest;
    }
}

#endif