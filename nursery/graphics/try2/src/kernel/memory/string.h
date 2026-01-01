#pragma once

void *memset(void *dest, int c, int size);
void *memcpy(void *dest, const void *src, int size);
int memcmp(const void *s1, const void *s2, int size);

int strlen(const char* str);
int strcmp(const char *a, const char *b);
void strcpy(char *target, const char *source);
void strcat(char *target, const char *source);