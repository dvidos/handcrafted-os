#ifndef _DIRENT_H
#define _DIRENT_H

#include <sys/types.h> // For ino_t

// --- Structure for a directory entry ---
struct dirent {
    ino_t d_ino;            // Inode number
    char  d_name[256];      // Null-terminated filename (max 255 chars)
    unsigned char d_type;   // File type

};

// Macros for d_type
#define DT_UNKNOWN       0
#define DT_FIFO          1
#define DT_CHR           2
#define DT_DIR           4
#define DT_BLK           6
#define DT_REG           8
#define DT_LNK          10
#define DT_SOCK         12

// Placeholder for DIR structure. A real implementation would define its internal members.
typedef struct __DIR DIR;

// --- Function Prototypes ---
DIR *opendir(const char *name);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);
int scandir(const char *dirp, struct dirent ***namelist,
            int (*filter)(const struct dirent *),
            int (*compar)(const struct dirent **, const struct dirent **));
void seekdir(DIR *dirp, long loc);
long telldir(DIR *dirp);

#endif // _DIRENT_H