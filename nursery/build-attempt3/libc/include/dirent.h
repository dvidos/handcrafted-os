#ifndef _DIRENT_H
#define _DIRENT_H

#include <sys/types.h> // For ino_t

// --- Structure for a directory entry ---
struct dirent {
    ino_t d_ino;            // Inode number
    char  d_name[256];      // Null-terminated filename (max 255 chars)
    // Add other fields like d_type if needed for full POSIX compliance
};

// Placeholder for DIR structure. A real implementation would define its internal members.
typedef struct __DIR DIR;

// --- Function Prototypes ---
DIR *opendir(const char *name);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);

#endif // _DIRENT_H