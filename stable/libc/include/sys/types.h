#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H

#include <kernel/base.h>
#include "../stddef.h" // For size_t and ptrdiff_t

// --- Pointer-sized integers ---
typedef uint32_t        uintptr_t;
typedef int32_t         intptr_t;

// --- Basic system types ---
typedef long            time_t;     // Keeping as long (32-bit) for i686 preference
typedef uint32_t        uid_t;
typedef uint32_t        gid_t;
typedef int32_t         pid_t;
typedef uint32_t        dev_t;      // Keeping as uint32_t for i686 preference
typedef uint32_t        ino_t;      // Keeping as uint32_t for i686 preference
typedef uint32_t        mode_t;
typedef int64_t         off_t;      // 64-bit for large file support
typedef int64_t         ssize_t;    // 64-bit for large file support
typedef uint32_t        nlink_t;


_Static_assert(sizeof(uint8_t)  == 1, "uint8_t is expected to have size of 1 byte");
_Static_assert(sizeof(uint16_t) == 2, "uint16_t is expected to have a size of 2 bytes");
_Static_assert(sizeof(uint32_t) == 4, "uint32_t is expected to have a size of 4 bytes");
_Static_assert(sizeof(uint64_t) == 8, "uint64_t is expected to have a size of 8 bytes");
_Static_assert(sizeof(void *)   == 4,   "pointer is expected to have a size of 4 bytes");
_Static_assert(sizeof(uintptr_t) == 4,   "uintptr_t is expected to have a size of 4 bytes");



#endif // _SYS_TYPES_H