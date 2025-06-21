#include <cstddef>
#include <cstdint>
#include <util/string.hpp>

void *memset(void *dst, int val, size_t length) {
    char *dstptr = (char *) dst;
    for (size_t i = 0; i < length; i++) {
        dstptr[i] = val;
    }

    return (void *) dst;
}

void *memset64(void *dst, int val, size_t length) {
    uint64_t *dstptr = (uint64_t *) dst;
    for (size_t i = 0; i < length / 8; i++) {
        dstptr[i] = val;
    }

    return (void *) dst;
}

void *memcpy(void *dst, const void *src, size_t length) {
    char *dstptr = (char *) dst;
    char *srcptr = (char *) src;
    for (size_t i = 0; i < length; i++) {
        dstptr[i] = srcptr[i];
    }

    return (void *) dst;
}

void *memmove(void *dst, const void *src, size_t length) {
    char *dstptr = (char *) dst;
    char *srcptr = (char *) src;

    if (dstptr == srcptr)
        return dstptr;

    if ((uintptr_t) srcptr - (uintptr_t) dstptr <= -2 * length)
        return memcpy(dst, src, length);

    if (dstptr < srcptr) {
        for (; length; length--)
            *dstptr++ = *srcptr++;
    } else {
        while(length) {
            length--;
            dstptr[length] = srcptr[length];
        }
    }

    return dstptr;
}

int memcmp(const void *a, const void *b, size_t length) {
    const unsigned char *as = (const unsigned char *) a;
    const unsigned char *bs = (const unsigned char *) b;

    for (size_t i = 0; i < length; i++, as++, bs++) {
        if (*as < *bs) {
            return -1;
        } else if (*as > *bs) {
            return 1;
        }
    }

    return 0;
}

size_t strncmp(const char *s1, const char *s2, size_t n) {
    while(n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        --n;
    }

    return !n ? 0 : *s1 - *s2;
}

int tolower(unsigned char ch) {
    if (ch >= 'A' && ch <= 'Z')
        ch = 'a' + (ch - 'A');
    return ch;
}

size_t strncasecmp(const char *s1, const char *s2, size_t n) {
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

size_t strcmp(const char *s1, const char *s2) {
    while(*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }

    return *s1 - *s2;
}

size_t strlen(const char *s) {
    size_t len = 0;
    char *src = (char *) s;
    while (*src) {
        len++;
        src++;
    }

    return len;
}

size_t strnlen(const char *s, size_t max) {
    size_t len = 0;
    char *src = (char *) s;
    while (*src && len <= max) {
        len++;
        src++;
    }

    return len;
}

char *strcpy(const char *dst, const char *src) {
    char *source = (char *) src;
    char *dest = (char *) dst;
    while ((*dest++ = *source++));
    
    return dest;
}