#ifndef _STRING_H
#define _STRING_H

#include <ctypes.h>
#include <va_list.h>


int memcmp(const void*, const void*, size_t);
void* memcpy(void*, const void*, size_t);
void* memmove(void*, const void*, size_t);
void* memset(void*, int, size_t);

size_t strlen(const char*);
void strcpy(char *target, char *source);
int strcmp(const char *a, const char *b);
char *strchr(char *str, char c);
char *strstr(char *haystack, char *needle);
char *strtok(char *str, char *delimiters);

void reverse(char *buffer, int len);

int tolower(int c);
int toupper(int c);



int atoi(char *str);
unsigned int atoui(char *str);
void ltoa(long num, char *buffer, int base);
void ultoa(unsigned long num, char *buffer, int base);


// always includes zero terminator
void sprintfn(char *buffer, int buffsize, const char *format, ...);

// always includes zero terminator
void vsprintfn(char *buffer, int buffsize, const char *format, va_list args);




// copies from offset onwards to buffer, advances offset. ignores leading and trainling slashes
// returns bytes extracted
int get_next_path_part(char *path, int *offset, char *buffer);

// returns how many parts in the path. leading and trailing '/' are ignored
int count_path_parts(char *path);

// gets the path part of index n (zero based)
// returns length of part copied - 0 if part not found
int get_n_index_path_part(char *path, int n, char *buffer);


#endif
