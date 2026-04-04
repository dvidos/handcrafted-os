#ifndef _INTTYPES_H
#define _INTTYPES_H

#include <stdint.h> // For fixed-width integer types (intmax_t, uintmax_t already there)

// Define intmax_t and uintmax_t if not already defined (e.g., in stdint.h)
// In most modern compilers, they are likely defined in stdint.h already.
#ifndef intmax_t
typedef int64_t intmax_t;
#endif

#ifndef uintmax_t
typedef uint64_t uintmax_t;
#endif

// --- Macros for printf/scanf format specifiers for fixed-width types ---

// Signed 8-bit
#define PRId8 "hhd"
#define PRIi8 "hhi"
// Unsigned 8-bit
#define PRIo8 "hho"
#define PRIu8 "hhu"
#define PRIx8 "hhx"
#define PRIX8 "hhX"

// Signed 16-bit
#define PRId16 "hd"
#define PRIi16 "hi"
// Unsigned 16-bit
#define PRIo16 "ho"
#define PRIu16 "hu"
#define PRIx16 "hx"
#define PRIX16 "hX"

// Signed 32-bit (assuming int is 32-bit)
#define PRId32 "d"
#define PRIi32 "i"
// Unsigned 32-bit
#define PRIo32 "o"
#define PRIu32 "u"
#define PRIx32 "x"
#define PRIX32 "X"

// Signed 64-bit (assuming long is 64-bit or long long exists)
#ifdef __LP64__ // For 64-bit systems where long is 64-bit
#define PRId64 "ld"
#define PRIi64 "li"
#define PRIo64 "lo"
#define PRIu64 "lu"
#define PRIx64 "lx"
#define PRIX64 "lX"
#else // For 32-bit systems where long long is 64-bit
#define PRId64 "lld"
#define PRIi64 "lli"
#define PRIo64 "llo"
#define PRIu64 "llu"
#define PRIx64 "llx"
#define PRIX64 "llX"
#endif

// --- Macros for printf/scanf format specifiers for intmax_t and uintmax_t ---
#define PRIdMAX "lld"
#define PRIiMAX "lli"
#define PRIoMAX "llo"
#define PRIuMAX "llu"
#define PRIxMAX "llx"
#define PRIXMAX "llX"

#endif // _INTTYPES_H