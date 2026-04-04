#ifndef _FCNTL_H
#define _FCNTL_H

#include <sys/types.h> // For mode_t

// --- Open File Flags ---
#define O_RDONLY    0x0000 // Open for reading only
#define O_WRONLY    0x0001 // Open for writing only
#define O_RDWR      0x0002 // Open for reading and writing

#define O_CREAT     0x0040 // Create file if it does not exist
#define O_EXCL      0x0080 // Exclusive use flag
#define O_NOCTTY    0x0100 // Do not assign controlling terminal
#define O_TRUNC     0x0200 // Truncate file to zero length if it exists
#define O_APPEND    0x0400 // Set append mode
#define O_NONBLOCK  0x0800 // Non-blocking I/O
#define O_SYNC      0x1000 // Write operations are synchronous

// Close-on-exec flag for fcntl()
#define FD_CLOEXEC  0x0001 // Close file descriptor on execve()

// fcntl() commands
#define F_DUPFD     0   // Duplicate file descriptor
#define F_GETFD     1   // Get file descriptor flags
#define F_SETFD     2   // Set file descriptor flags
#define F_GETFL     3   // Get file status flags
#define F_SETFL     4   // Set file status flags

// --- Function Prototypes ---
int open(const char *pathname, int flags, ...);
int creat(const char *pathname, mode_t mode);
int fcntl(int fd, int cmd, ...);

#endif // _FCNTL_H