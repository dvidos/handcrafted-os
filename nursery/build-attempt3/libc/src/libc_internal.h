#ifndef _LIBC_INTERNAL_H
#define _LIBC_INTERNAL_H


#include "../include/sys/stat.h"
#include "../include/sys/time.h"
#include "../include/sys/types.h"
#include "../include/sys/wait.h"

#include "../include/assert.h"
#include "../include/ctype.h"
#include "../include/dirent.h"
#include "../include/errno.h"
#include "../include/fcntl.h"
#include "../include/float.h"
#include "../include/inttypes.h"
#include "../include/limits.h"
#include "../include/math.h"
#include "../include/setjmp.h"
#include "../include/signal.h"
#include "../include/stdarg.h"
#include "../include/stdbool.h"
#include "../include/stddef.h"
#include "../include/stdint.h"
#include "../include/stdio.h"
#include "../include/stdlib.h"
#include "../include/string.h"
#include "../include/time.h"
#include "../include/unistd.h"
#include "../include/utime.h"

// syscall numbers
#include "../../kernel/include/uapi/syscall.h"
int syscall(int sysno, int arg1, int arg2, int arg3, int arg4, int arg5);

// syslog macros and stuff
#include "../include/hcos/syslog.h"


// yield, spawn and other non-posix
#include "../include/hcos/misc.h"


// For DIR and dirent structures
#include "../include/dirent.h"
#include "../include/kernel/vfs_dirent.h"

typedef struct _IO_FILE {
    int fd;             // The underlying kernel file descriptor
    char *buffer;       // Pointer to the heap-allocated buffer
    int buf_size;       // Usually 1024 or 4096 (BUFSIZ)
    int pos;            // Current read/write position in the buffer
    int end;            // End of valid data in the buffer (for reading)
    int flags;          // Error flags, EOF, etc.
    int ungetc_char;    // Character pushed back by ungetc
    bool has_ungetc_char; // Flag if ungetc_char is valid
    struct _IO_FILE *next;  // allow chaining for flushing buffers at end.
} FILE;

typedef struct __DIR {
    int fd; // File descriptor for the open directory
    struct dirent entry; // Buffer for the current directory entry
} DIR;

extern FILE *__open_files_list;

// Function to flush all open files
void __flush_all_files(void);

// Internal flags for FILE structure
#define _IO_READ        (1 << 0) // File is open for reading
#define _IO_WRITE       (1 << 1) // File is open for writing
#define _IO_APPEND      (1 << 2) // File is open for appending
#define _IO_BINARY      (1 << 3) // File is open in binary mode
#define _IO_EOF         (1 << 4) // End-of-file indicator set
#define _IO_ERROR       (1 << 5) // Error indicator set
#define _IO_NO_BUF      (1 << 6) // No buffering
#define _IO_LINE_BUF    (1 << 7) // Line buffering
#define _IO_FULL_BUF    (1 << 8) // Full buffering









#endif // _LIBC_INTERNAL_H