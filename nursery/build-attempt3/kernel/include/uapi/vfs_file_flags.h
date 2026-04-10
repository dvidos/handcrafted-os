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
    #define S_IFMT   0170000  // mask for file type bits
    #define S_IFREG  0100000  // regular file
    #define S_IFDIR  0040000  // directory
    #define S_IFCHR  0020000  // character device
    #define S_IFBLK  0060000  // block device
    #define S_IFIFO  0010000  // FIFO / named pipe
    #define S_IFLNK  0120000  // symbolic link

    // Owner permissions
    #define S_IRUSR  00400  // owner can read
    #define S_IWUSR  00200  // owner can write
    #define S_IXUSR  00100  // owner can execute

    // Group permissions
    #define S_IRGRP  00040  // group can read
    #define S_IWGRP  00020  // group can write
    #define S_IXGRP  00010  // group can execute

    // Others permissions
    #define S_IROTH  00004  // others can read
    #define S_IWOTH  00002  // others can write
    #define S_IXOTH  00001  // others can execute


#endif