#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>


int strlen(const char* str);
int strcmp(char *a, char *b);

void memset(char *dest, uint8_t value, size_t size);
void memcpy(char *dest, char *source, size_t size);

void reverse(char *buffer, int len);
void ltoa(long num, char *buffer, int base);
void ultoa(unsigned long num, char *buffer, int base);

char *strchr(char *str, char c);
char *strtok(char *str, char *delimiters);
int atoi(char *buffer);


#endif
