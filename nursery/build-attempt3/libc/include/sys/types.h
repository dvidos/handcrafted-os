#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H

#include <stddef.h> // For size_t and ptrdiff_t

// --- Standard Integer Types ---
typedef unsigned char   uint8_t;
typedef signed char     int8_t;
typedef unsigned short  uint16_t;
typedef signed short    int16_t;
typedef unsigned int    uint32_t;
typedef signed int      int32_t;
typedef unsigned long   uint64_t; // Assuming 64-bit long for now, adjust as needed for target arch
typedef signed long     int64_t;  // Assuming 64-bit long for now, adjust as needed for target arch

// --- Pointer-sized integers ---
typedef unsigned long   uintptr_t; // Assuming uintptr_t is unsigned long
typedef signed long     intptr_t;  // Assuming intptr_t is signed long

// --- Basic system types ---
typedef long            time_t;     // Used for time values
typedef unsigned int    uid_t;      // User ID
typedef unsigned int    gid_t;      // Group ID
typedef int             pid_t;      // Process ID
typedef unsigned int    dev_t;      // Device ID
typedef unsigned long   ino_t;      // Inode number
typedef unsigned int    mode_t;     // File mode (permissions and type)
typedef long            off_t;      // File offset
typedef long            ssize_t;    // Signed size_t, used for byte counts or errors

#endif // _SYS_TYPES_H