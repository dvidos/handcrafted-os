#ifndef _STRING_H
#define _STRING_H

#include <ctypes.h>


int memcmp(const void*, const void*, size_t);
void* memcpy(void*, const void*, size_t);
void* memmove(void*, const void*, size_t);
void* memset(void*, int, size_t);

size_t strlen(const char*);
int strcmp(const char *a, const char *b);

#endif
