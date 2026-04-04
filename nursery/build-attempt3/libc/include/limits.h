#ifndef _LIMITS_H
#define _LIMITS_H

// --- Character properties ---
#define CHAR_BIT    8           // Number of bits in a char

#define SCHAR_MIN   (-128)      // Minimum value for a signed char
#define SCHAR_MAX   127         // Maximum value for a signed char
#define UCHAR_MAX   255         // Maximum value for an unsigned char

// CHAR_MIN and CHAR_MAX depend on whether char is signed or unsigned
#ifdef __CHAR_UNSIGNED__
#define CHAR_MIN    0
#define CHAR_MAX    UCHAR_MAX
#else
#define CHAR_MIN    SCHAR_MIN
#define CHAR_MAX    SCHAR_MAX
#endif

// --- Integer limits ---
#define SHRT_MIN    (-32768)    // Minimum value for a short int
#define SHRT_MAX    32767       // Maximum value for a short int
#define USHRT_MAX   65535       // Maximum value for an unsigned short int

#define INT_MIN     (-2147483647 - 1) // Minimum value for an int
#define INT_MAX     2147483647        // Maximum value for an int
#define UINT_MAX    4294967295U       // Maximum value for an unsigned int

#define LONG_MIN    (-2147483647L - 1L) // Minimum value for a long int (assuming 32-bit long)
#define LONG_MAX    2147483647L         // Maximum value for a long int (assuming 32-bit long)
#define ULONG_MAX   4294967295UL        // Maximum value for an unsigned long int (assuming 32-bit long)

// If long is 64-bit, uncomment and use these:
// #define LONG_MIN    (-9223372036854775807L - 1L)
// #define LONG_MAX    9223372036854775807L
// #define ULONG_MAX   18446744073709551615UL

#define LLONG_MIN   (-9223372036854775807LL - 1LL) // Minimum value for a long long int
#define LLONG_MAX   9223372036854775807LL         // Maximum value for a long long int
#define ULLONG_MAX  18446744073709551615ULL       // Maximum value for an unsigned long long int

#endif // _LIMITS_H