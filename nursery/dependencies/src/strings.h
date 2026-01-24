#pragma once
#include "fundamentals.h"

#ifdef HOSTED

    // bring stdc implementations
    #include <string.h>
    #include <stdio.h>

#else
#ifdef STANDALONE

    // we need our own versions
    #include <stdarg.h>

    // we need our own versions
    size_t strlen(const char *s);
    void strcpy(char *dest, const char *src);
    void strcat(char *dest, const char *src);
    int strcmp(const char *a, const char *b);

    void memcpy(void *dest, const void *src, size_t len);
    void memset(void *dest, char value, size_t len);
    int memcmp(const void *a, const void *b, size_t len);

    void ultoa(unsigned long num, char *buffer, int base);
    void ltoa(long num, char *buffer, int base);

    int snprintf(char *buffer, int buffer_size, const char *fmt, ...);
    int vsnprintf(char *buffer, int buffer_size, const char *fmt, va_list args);

#endif
#endif