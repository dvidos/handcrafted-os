#ifndef _SYSCALL_H
#define _SYSCALL_H


#ifdef __is_libc
    int syscall(int sysno, int arg1, int arg2, int arg3, int arg4, int arg5);
#endif


// number of syscall method -- to be shared with kernel.
#define SYS_ECHO_TEST         1  // test, returns arg 1
#define SYS_ADD_TEST          2  // test, returns sum of args 1 - 5

// we don't have kernel log in user land...
#define SYS_LOG_ENTRY         3  // log something in syslog. 
#define SYS_LOG_HEX_DUMP      4  // log binary contents in syslog

// pseudo-tty manipulation
#define SYS_PUTS               11  // arg1 = string
#define SYS_PUTCHAR            12  // arg1 = char
#define SYS_CLEAR_SCREEN       13  // no args
#define SYS_WHERE_XY           14  // returns x, y in hi/lo byte, zero based
#define SYS_GOTO_XY            15  // arg1 = x, arg2 = y, zero based
#define SYS_SCREEN_DIMENSIONS  16  // returns cols/rows in hi/lo byte
#define SYS_GET_SCREEN_COLOR   17  // arg1 = color, returns old color
#define SYS_SET_SCREEN_COLOR   18  // arg1 = color, returns old color
#define SYS_GET_KEY_EVENT      19  // returns... a lot of info (we have 4 bytes)
#define SYS_GET_MOUSE_EVENT    20  // returns... a lot of info (we have 4 bytes)

// files manipulation
#define SYS_OPEN             31  // arg1 = file path, returns handle or error<0
#define SYS_READ             32  // arg1 = handle, arg2 = buffer, arg3 = len, returns len
#define SYS_WRITE            33  // arg1 = handle, arg2 = buffer, arg3 = len, returns len
#define SYS_SEEK             34  // arg1 = handle, arg2 = offset, arg3 = origin, returns new position
#define SYS_CLOSE            35  // arg1 = handle
#define SYS_OPEN_DIR         36  // arg1 = dir path, return handle or error<0
#define SYS_READ_DIR         37  // arg1 = handle, arg2 = dentry pointer
#define SYS_CLOSE_DIR        38  // arg1 = handle
#define SYS_TOUCH            39  // arg1 = path
#define SYS_MKDIR            40  // arg1 = path
#define SYS_UNLINK           41  // arg1 = path (dir or file)

// process manipulation
#define SYS_GET_CWD          51  // arg1 = buffer, arg2 = buffer size
#define SYS_SET_CWD          52  // arg1 = path
#define SYS_GET_PID          53  // returns pid
#define SYS_GET_PPID         54  // returns ppid
#define SYS_FORK             55  // returns 0 in child, child PID in parent, neg error in parent
#define SYS_WAIT_CHILD       56  // returns child's PID, arg1 = pointer to write exit_code
#define SYS_EXEC             56  // arg1 = path, arg2 = argv, arg3 = envp, returns... maybe?
#define SYS_SLEEP            57  // arg1 = millisecs
#define SYS_YIELD            58  // no args
#define SYS_EXIT             59  // arg1 = exit code
#define SYS_SBRK             60  // arg1 = signed desired diff, returns pointer to new area

// clock info
#define SYS_GET_UPTIME       80  // returns msecs since boot (32 bits = 49 days)
#define SYS_GET_CLOCK        81  // arg1 = dtime pointer

// IPC send, receive, shared memory

// networking? sockets? where is all the fun?





#endif
