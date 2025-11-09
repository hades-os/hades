#ifndef STRING_HPP
#define STRING_HPP

#include <cstddef>
#include <cstdint>

extern "C" {
    void *memset(void *dst, int val, size_t length);
    void *memset64(void *dst, int val, size_t length);
    void *memcpy(void *dst, const void *src, size_t length);
    int memcmp(const void *a, const void *b, size_t length);

    size_t strncmp(const char *s1, const char *s2, size_t n);
    int tolower(unsigned char ch);
    size_t strncasecmp(const char *s1, const char *s2, size_t n);
    size_t strcmp(const char *s1, const char *s2);
    size_t strlen(const char *s);
    char *strcpy(const char *dst, const char *src);
}

#endif