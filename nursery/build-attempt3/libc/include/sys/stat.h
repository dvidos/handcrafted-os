#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include "types.h" // For dev_t, ino_t, mode_t, nlink_t, uid_t, gid_t, off_t, time_t

// --- Structure for file status ---
struct stat {
    dev_t     st_dev;     // ID of device containing file
    ino_t     st_ino;     // Inode number
    mode_t    st_mode;    // File type and mode (permissions)
    nlink_t   st_nlink;   // Number of hard links
    uid_t     st_uid;     // User ID of owner
    gid_t     st_gid;     // Group ID of owner
    dev_t     st_rdev;    // Device ID (if special file)
    off_t     st_size;    // Total size, in bytes
    time_t    st_atime;   // Time of last access
    time_t    st_mtime;   // Time of last modification
    time_t    st_ctime;   // Time of last status change
    long      st_blksize; // Block size for filesystem I/O
    long      st_blocks;  // Number of 512B blocks allocated
};

// --- File type macros (part of st_mode) ---
#define S_IFMT      0xF000      // Mask for file type
#define S_IFDIR     0x4000      // Directory
#define S_IFCHR     0x2000      // Character device
#define S_IFBLK     0x6000      // Block device
#define S_IFREG     0x8000      // Regular file
#define S_IFIFO     0x1000      // FIFO (named pipe)
#define S_IFLNK     0xA000      // Symbolic link
#define S_IFSOCK    0xC000      // Socket

// Macro to check file type
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

// --- File permission bits (part of st_mode) ---
#define S_IRWXU     00700       // RWE for owner
#define S_IRUSR     00400       // Read by owner
#define S_IWUSR     00200       // Write by owner
#define S_IXUSR     00100       // Execute by owner

#define S_IRWXG     00070       // RWE for group
#define S_IRGRP     00040       // Read by group
#define S_IWGRP     00020       // Write by group
#define S_IXGRP     00010       // Execute by group

#define S_IRWXO     00007       // RWE for others
#define S_IROTH     00004       // Read by others
#define S_IWOTH     00002       // Write by others
#define S_IXOTH     00001       // Execute by others

// --- Function Prototypes ---
int stat(const char *pathname, struct stat *buf);
int fstat(int fd, struct stat *buf);
int lstat(const char *pathname, struct stat *buf);
int chmod(const char *pathname, mode_t mode);
int fchmod(int fd, mode_t mode);
int fchown(int fd, uid_t owner, gid_t group);
int mkdir(const char *pathname, mode_t mode);
int mkfifo(const char *pathname, mode_t mode);
mode_t umask(mode_t mask);

#endif // _SYS_STAT_H