#ifndef _KERNEL_SYSCALL_H
#define _KERNEL_SYSCALL_H


// number of syscall method -- to be shared with kernel.
#define SYS_ECHO_TEST         1   // test, returns arg 1
#define SYS_ADD_TEST          2   // test, returns sum of args 1 - 5

// logging
#define SYS_LOG_ENTRY         3   // log something in syslog
#define SYS_LOG_HEX_DUMP      4   // log binary contents in syslog

// file manipulation
#define SYS_OPEN             21   // arg1 = file path, returns handle or error<0
#define SYS_READ             22   // arg1 = handle, arg2 = buffer, arg3 = len
#define SYS_WRITE            23   // arg1 = handle, arg2 = buffer, arg3 = len
#define SYS_SEEK             24   // arg1 = handle, arg2 = offset, arg3 = origin
#define SYS_CLOSE            25   // arg1 = handle
#define SYS_OPEN_DIR         26   // arg1 = dir path
#define SYS_READ_DIR         27   // arg1 = handle
#define SYS_REWIND_DIR       28   // arg1 = handle
#define SYS_CLOSE_DIR        29   // arg1 = handle
#define SYS_TOUCH            30   // arg1 = path
#define SYS_UNLINK           31   // arg1 = path
#define SYS_MKDIR            32   // arg1 = path
#define SYS_RMDIR            33   // arg1 = path

// file metadata / control
#define SYS_STAT             41   // arg1 = path, arg2 = struct stat*
#define SYS_FSTAT            42   // arg1 = fd, arg2 = struct stat*
#define SYS_FCNTL            43   // arg1 = fd, arg2 = cmd, arg3 = arg
#define SYS_RENAME           44   // arg1 = old path, arg2 = new path
#define SYS_IOCTL            45   // arg1 = handle, arg2 = cmd, arg3 = arg

// process / environment
#define SYS_GET_CWD          51   // arg1 = buffer, arg2 = buffer size
#define SYS_CHDIR            52   // arg1 = path
#define SYS_GET_PID          53   // returns pid
#define SYS_GET_PPID         54   // returns ppid
#define SYS_FORK             55   // returns 0 in child, child PID in parent
#define SYS_WAIT_ANY_CHILD   56   // arg1 = pointer to exit_code
#define SYS_WAIT_SPEC_CHILD  57   // arg1 = child_pid, arg2 = pointer to exit code
#define SYS_EXEC             58   // arg1 = path, arg2 = argv, arg3 = envp
#define SYS_SPAWN            59   // arg1 = path, arg2 = argv, arg3 = envp
#define SYS_SLEEP            60   // arg1 = millisecs
#define SYS_YIELD            61   // no args
#define SYS_EXIT             62   // arg1 = exit code
#define SYS_SBRK             63   // arg1 = signed diff, returns pointer

// fd / IPC
#define SYS_DUP              64   // arg1 = old fd
#define SYS_DUP2             65   // arg1 = old fd, arg2 = new fd
#define SYS_PIPE             66   // returns two fds (read, write)

// signals (optional)
#define SYS_KILL             68   // arg1 = pid, arg2 = signal
#define SYS_SIGNAL           69   // arg1 = signal, arg2 = handler

// mounting (optional)
#define SYS_MOUNT            70   // arg1 = source, arg2 = target, arg3 = fs type
#define SYS_UMOUNT           71   // arg1 = target

// clock info
#define SYS_GET_UPTIME       80   // returns msecs since boot
#define SYS_GET_CLOCK        81   // arg1 = dtime pointer




// IPC send, receive, shared memory
// networking? sockets? where is all the fun?



#endif
