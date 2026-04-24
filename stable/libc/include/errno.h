#ifndef _ERRNO_H
#define _ERRNO_H


#include "kernel/errors.h"


// Declare errno as an external integer. Its value is set by library functions
// on error and is not cleared on success.
extern int errno;

// For thread-safe access to errno
extern int *__errno_location(void);

// --- Common POSIX Error Codes ---
#define EPERM     ERR_NOT_PERMITTED
#define ENOENT    ERR_NOT_FOUND
#define ESRCH     ERR_NOT_FOUND
#define EINTR     ERR_INTERRUPTED
#define EIO       ERR_IO_ERROR
#define ENXIO     ERR_NO_DEVICE
#define E2BIG     ERR_TOO_LONG
#define ENOEXEC   ERR_BAD_EXECUTABLE
#define EBADF     ERR_BAD_FILE
#define ECHILD    ERR_NO_CHILDREN
#define EAGAIN    ERR_AGAIN
#define ENOMEM    ERR_NO_MEMORY
#define EACCES    ERR_ACCESS_DENIED
#define EFAULT    ERR_BAD_ADDRESS
#define ENOTBLK   ERR_BLOCK_DEVICE_NEEDED
#define EBUSY     ERR_BUSY
#define EEXIST    ERR_ALREADY_EXISTS
#define ENODEV    ERR_NO_DEVICE
#define ENOTDIR   ERR_NOT_A_DIRECTORY
#define EISDIR    ERR_IS_A_DIRECTORY
#define EINVAL    ERR_BAD_ARGUMENT
#define EMFILE    ERR_TOO_MANY_OPEN_FILES
#define ENOTTY    ERR_NOT_A_TTY
#define EFBIG     ERR_FILE_TOO_LARGE
#define ENOSPC    ERR_NO_SPACE_LEFT
#define EROFS     ERR_READ_ONLY_SYSTEM
#define EPIPE     ERR_BROKEN_PIPE
#define ENOSYS    ERR_NOT_IMPLEMENTED
#define ERANGE    ERR_OUT_OF_RANGE
#define EOVERFLOW ERR_OVERFLOWN




#endif // _ERRNO_H