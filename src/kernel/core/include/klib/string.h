#ifndef _STRING_H
#define _STRING_H

#include <ctypes.h>
#include <va_list.h>

int strlen(const char* str);
int strcmp(char *a, char *b);
int strcpy(char *target, char *source);
void strncpy(char *target, char *source, size_t target_size);

char *strchr(char *str, char c);
char *strstr(char *heystack, char *needle);
char *strtok(char *str, char *delimiters);

void memset(void *dest, char value, size_t size);
void memcpy(void *dest, void *source, size_t size);
int  memcmp(void *a, void *b, size_t size);
char *memmove(void *dest, void *source, size_t size);

// check if a memory area consists only of one character
bool memchk(void *buffer, char value, size_t size);

void reverse(char *buffer, int len);
void ltoa(long num, char *buffer, int base);
void ultoa(unsigned long num, char *buffer, int base);

int atoi(char *buffer);
unsigned int atoui(char *buffer);

void sprintfn(char *buffer, int buffsize, const char *format, ...);
void vsprintfn(char *buffer, int buffsize, const char *format, va_list args);

int tolower(int c);
int toupper(int c);

void ucs2str_to_cstr(char *ucs2str, char *cstr);


#endif
