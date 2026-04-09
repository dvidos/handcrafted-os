#ifndef _STRING_H
#define _STRING_H

#include <stddef.h> // For size_t, NULL

// --- Function Prototypes (based on usage analysis) ---

// Copying
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
size_t strlcpy(char *dst, const char *src, size_t dsize); // BSD extension

// Concatenation
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t n);
size_t strlcat(char *dst, const char *src, size_t dsize); // BSD extension

// Comparison
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
int strcasecmp(const char *s1, const char *s2); // Non-standard but very common

// Searching
size_t strlen(const char *s);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strpbrk(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);
char *strstr(const char *haystack, const char *needle);
char *strtok(char *str, const char *delim);
char *strtok_r(char *str, const char *delim, char **saveptr);

// Memory manipulation
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memccpy(void *restrict dst, const void *restrict src, int c, size_t n);
void *memset(void *s, int c, size_t n);
void *memchr(const void *s, int c, size_t n);

// Miscellaneous
char *strerror(int errnum);
char *strerror_r(int errnum, char *buf, size_t buflen);
char *strdup(const char *s); // POSIX extension
size_t strxfrm(char *dest, const char *src, size_t n);

#endif // _STRING_H