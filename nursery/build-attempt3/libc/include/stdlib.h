#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h> // For NULL, size_t

// --- Macros ---
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 32767 // Minimum guaranteed value for RAND_MAX

// --- Function Prototypes (based on usage analysis) ---

// Memory management
void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);

// Process control
void abort(void);
void exit(int status);
char *getenv(const char *name);
int putenv (char *string); // data will used as is, not copied
int system(const char *command);

// String conversion
int atoi(const char *str);
long atol(const char *str);
long long atoll(const char *str);
long strtol(const char *str, char **endptr, int base);
long long strtoll(const char *str, char **endptr, int base);
unsigned long strtoul(const char *str, char **endptr, int base);
unsigned long long strtoull(const char *str, char **endptr, int base);

// Random numbers
int rand(void);
void srand(unsigned int seed);

// Integer arithmetic
int abs(int j);
long labs(long j);
long long llabs(long long j);

// Searching and sorting
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));

#endif // _STDLIB_H