#ifndef _STRING_H
#define _STRING_H

int strlen(const char* str);

void memset(char *dest, uint8_t value, size_t size);

void reverse(char *buffer, int len);
void ltoa(long num, char *buffer, int base);
void ultoa(unsigned long num, char *buffer, int base);


#endif
