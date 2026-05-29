#pragma once

// POSIX-related flags for open(), create() etc.
#ifndef __HOST_SYSTEM__


    // ----------- Open flags below -----------------------

    // Access mode (exactly one must be set)
    #define O_RDONLY    0x0000  // open for read only
    #define O_WRONLY    0x0001  // open for write only
    #define O_RDWR      0x0002  // open for read and write
    #define O_ACCMODE   0x0003  // mask to extract access mode

    // Creation / open behavior
    #define O_CREAT     0x0040  // create file if it does not exist
    #define O_EXCL      0x0080  // with O_CREAT, fail if file already exists
    #define O_TRUNC     0x0200  // truncate file to zero length on open
    #define O_APPEND    0x0400  // force writes to append at end of file

    // Open-time behavior modifiers (optional / future)
    #define O_NONBLOCK  0x0800  // do not block on open/read/write
    #define O_SYNC      0x1000  // writes complete before returning
    #define O_CLOEXEC   0x2000  // close file on exec()

    // ----------------- File type flags below ----------------------

    // 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
    //                                                 File        Set     Permissions
    //                                                 Type        Sticky  Owner Group Other
    //                                                 .. .. .. .. SU SG S R W X R W X R W X

    // used by create(), mkdir(), mknod(), stat.st_mode
    // File type (pick only one)
    #define S_IFMT     0170000  // [0xF000, 61440] mask for file type bits
    #define S_IRWXUGO  0000777  // [0x1FF,    511] mask for file permission bits

    // file types
    #define S_IFREG    0100000  // [0x8000, 32768] regular file
    #define S_IFDIR    0040000  // [0x4000, 16384] directory
    #define S_IFCHR    0020000  // [0x2000,  8192] character device
    #define S_IFBLK    0060000  // [0x6000, 24576] block device
    #define S_IFIFO    0010000  // [0x1000,  4096] FIFO / named pipe
    #define S_IFLNK    0120000  // [0xA000, 40960] symbolic link

    // Owner permissions
    #define S_IRUSR    0000400  // [0x0100,  256] owner can read
    #define S_IWUSR    0000200  // [0x0080,  128] owner can write
    #define S_IXUSR    0000100  // [0x0040,   64] owner can execute

    // Group permissions
    #define S_IRGRP    0000040  // [0x0020,  32] group can read
    #define S_IWGRP    0000020  // [0x0010,  16] group can write
    #define S_IXGRP    0000010  // [0x0008,   8] group can execute

    // Others permissions
    #define S_IROTH    0000004  // [0x0004,  4] others can read
    #define S_IWOTH    0000002  // [0x0002,  2] others can write
    #define S_IXOTH    0000001  // [0x0001,  1] others can execute

    // Convenience macros
    #define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
    #define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
    #define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
    #define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
    #define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
    #define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)

    // ----------- Access modes below -----------------------
    #define F_OK        0       /* Test for existence.  */
    #define X_OK        1       /* Test for execute permission.  */
    #define W_OK        2       /* Test for write permission.  */
    #define R_OK        4       /* Test for read permission.  */

#endif