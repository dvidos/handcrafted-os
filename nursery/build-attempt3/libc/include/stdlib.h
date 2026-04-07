#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h> // For NULL, size_t

// --- Type Definitions ---
typedef struct {
    int quot; // quotient
    int rem;  // remainder
} div_t;

typedef struct {
    long quot; // quotient
    long rem;  // remainder
} ldiv_t;

typedef struct {
    long long quot; // quotient
    long long rem;  // remainder
} lldiv_t;

// --- Macros ---
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 32767 // Minimum guaranteed value for RAND_MAX



// Memory management
#include "hcos/malloc_extensions.h"
// void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);
void *aligned_alloc(size_t alignment, size_t size); // C11

// Process control
void abort(void);
void exit(int status);
char *getenv(const char *name);
int putenv (char *string); // data will used as is, not copied
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
int system(const char *command);

// String conversion
double atof(const char *str);
double strtod(const char *str, char **endptr);
int atoi(const char *str);
long atol(const char *str);
long long atoll(const char *str);
long strtol(const char *str, char **endptr, int base);
long long strtoll(const char *str, char **endptr, int base);
unsigned long strtoul(const char *str, char **endptr, int base);
unsigned long long strtoull(const char *str, char **endptr, int base);
char *realpath(const char *path, char *resolved_path);

// Non standard extensions
char *itoa(int value, char *str, int base);
char *ltoa(long value, char *str, int base);
char *ultoa(unsigned long value, char *str, int base);
char *lltoa(long long value, char *str, int base);
char *ulltoa(unsigned long long value, char *str, int base);

// Random numbers
int rand(void);
void srand(unsigned int seed);

// Integer arithmetic
int abs(int j);
long labs(long j);
long long llabs(long long j);
div_t div(int numer, int denom);
ldiv_t ldiv(long numer, long denom);
lldiv_t lldiv(long long numer, long long denom);

// Searching and sorting
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));

#endif // _STDLIB_H