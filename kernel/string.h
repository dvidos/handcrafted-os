#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>
#include <stdarg.h>


int strlen(const char* str);
int strcmp(char *a, char *b);
int strcpy(char *target, char *source);

char *strchr(char *str, char c);
char *strtok(char *str, char *delimiters);

void memset(void *dest, uint8_t value, size_t size);
void memcpy(void *dest, void *source, size_t size);
int  memcmp(void *a, void *b, size_t size);
char *memmove(void *dest, void *source, size_t size);

void reverse(char *buffer, int len);
void ltoa(long num, char *buffer, int base);
void ultoa(unsigned long num, char *buffer, int base);
int atoi(char *buffer);

void sprintfn(char *buffer, int buffsize, const char *format, ...);
void vsprintfn(char *buffer, int buffsize, const char *format, va_list args);


#endif
