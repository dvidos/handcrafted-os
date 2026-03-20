#ifndef _KERNEL_SYSCALL_H
#define _KERNEL_SYSCALL_H


// number of syscall method -- to be shared with kernel.
#define SYS_ECHO_TEST         1   // test, returns arg 1
#define SYS_ADD_TEST          2   // test, returns sum of args 1 - 5

// logging
#define SYS_LOG_ENTRY         3   // log something in syslog
#define SYS_LOG_HEX_DUMP      4   // log binary contents in syslog

// file manipulation
#define SYS_OPEN             31   // arg1 = file path, returns handle or error<0
#define SYS_READ             32   // arg1 = handle, arg2 = buffer, arg3 = len
#define SYS_WRITE            33   // arg1 = handle, arg2 = buffer, arg3 = len
#define SYS_SEEK             34   // arg1 = handle, arg2 = offset, arg3 = origin
#define SYS_CLOSE            35   // arg1 = handle
#define SYS_OPEN_DIR         36   // arg1 = dir path
#define SYS_READ_DIR         37   // arg1 = handle
#define SYS_REWIND_DIR       38   // arg1 = handle
#define SYS_CLOSE_DIR        39   // arg1 = handle
#define SYS_TOUCH            40   // arg1 = path
#define SYS_UNLINK           41   // arg1 = path
#define SYS_MKDIR            42   // arg1 = path
#define SYS_RMDIR            43   // arg1 = path

// file metadata / control
#define SYS_STAT             44   // arg1 = path, arg2 = struct stat*
#define SYS_FSTAT            45   // arg1 = fd, arg2 = struct stat*
#define SYS_FCNTL            46   // arg1 = fd, arg2 = cmd, arg3 = arg
#define SYS_RENAME           47   // arg1 = old path, arg2 = new path

// process / environment
#define SYS_GET_CWD          51   // arg1 = buffer, arg2 = buffer size
#define SYS_CHDIR            52   // arg1 = path
#define SYS_GET_PID          53   // returns pid
#define SYS_GET_PPID         54   // returns ppid
#define SYS_FORK             55   // returns 0 in child, child PID in parent
#define SYS_WAIT_CHILD       56   // arg1 = pointer to exit_code
#define SYS_EXEC             57   // arg1 = path, arg2 = argv, arg3 = envp
#define SYS_SPAWN            58   // arg1 = path, arg2 = argv, arg3 = envp
#define SYS_SLEEP            59   // arg1 = millisecs
#define SYS_YIELD            60   // no args
#define SYS_EXIT             61   // arg1 = exit code
#define SYS_SBRK             62   // arg1 = signed diff, returns pointer

// fd / IPC
#define SYS_DUP              63   // arg1 = old fd
#define SYS_DUP2             64   // arg1 = old fd, arg2 = new fd
#define SYS_PIPE             65   // returns two fds (read, write)
#define SYS_IOCTL            66   // arg1 = fd, arg2 = request, arg3 = arg

// signals (optional)
#define SYS_KILL             67   // arg1 = pid, arg2 = signal
#define SYS_SIGNAL           68   // arg1 = signal, arg2 = handler

// mounting (optional)
#define SYS_MOUNT            70   // arg1 = source, arg2 = target, arg3 = fs type
#define SYS_UMOUNT           71   // arg1 = target

// clock info
#define SYS_GET_UPTIME       80   // returns msecs since boot
#define SYS_GET_CLOCK        81   // arg1 = dtime pointer




// IPC send, receive, shared memory
// networking? sockets? where is all the fun?



#endif
