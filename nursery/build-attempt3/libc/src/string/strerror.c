#include "../libc_internal.h"
#include <string.h> // Not strictly needed for this implementation, but keeping for consistency
#include <errno.h>  // For E* macros
#include <stddef.h> // For size_t, which might be implicitly used by some headers

// This array holds the error messages.
// The index corresponds to the errno value.
// It's a simplified version for a basic libc, matching the E* definitions in errno.h
static const char *const _sys_errlist[] = {
    "Success",                  // 0
    "Operation not permitted",  // EPERM       1
    "No such file or directory",// ENOENT      2
    "No such process",          // ESRCH       3
    "Interrupted system call",  // EINTR       4
    "I/O error",                // EIO         5
    "No such device or address",// ENXIO       6
    "Argument list too long",   // E2BIG       7
    "Exec format error",        // ENOEXEC     8
    "Bad file number",          // EBADF       9
    "No child processes",       // ECHILD     10
    "Resource temporarily unavailable", // EAGAIN     11 (often same as EWOULDBLOCK)
    "Out of memory",            // ENOMEM     12
    "Permission denied",        // EACCES     13
    "Bad address",              // EFAULT     14
    "Block device required",    // ENOTBLK    15
    "Device or resource busy",  // EBUSY      16
    "File exists",              // EEXIST     17
    "Cross-device link",        // EXDEV      18
    "No such device",           // ENODEV     19
    "Not a directory",          // ENOTDIR    20
    "Is a directory",           // EISDIR     21
    "Invalid argument",         // EINVAL     22
    "File table overflow",      // ENFILE     23
    "Too many open files",      // EMFILE     24
    "Not a typewriter",         // ENOTTY     25
    "Text file busy",           // ETXTBSY    26
    "File too large",           // EFBIG      27
    "No space left on device",  // ENOSPC     28
    "Illegal seek",             // ESPIPE     29
    "Read-only file system",    // EROFS      30
    "Too many links",           // EMLINK     31
    "Broken pipe",              // EPIPE      32
    "Math argument out of domain of func", // EDOM 33
    "Math result not representable", // ERANGE 34
    "Deadlock avoided",         // EDEADLK    35 (placeholder)
    "File name too long",       // ENAMETOOLONG 36 (placeholder)
    "No record locks available",// ENOLCK     37 (placeholder)
    "Function not implemented", // ENOSYS     38
    "Directory not empty",      // ENOTEMPTY  39 (placeholder)
    "Too many symbolic links encountered", // ELOOP 40
    // ... add more as needed, ensuring indices match errno values
};

#define _SYS_NERR (int)(sizeof(_sys_errlist) / sizeof(_sys_errlist[0]))

char *strerror(int errnum) {
    if (errnum >= 0 && errnum < _SYS_NERR) {
        return (char *)_sys_errlist[errnum];
    } else {
        // Fallback for unknown error numbers
        // A more robust implementation might use a static buffer and snprintf
        // for "Unknown error %d" to conform more closely to POSIX,
        // but for a basic libc, a generic string is acceptable.
        return "Unknown error";
    }
}