#ifndef _UTIME_H
#define _UTIME_H

#include <sys/types.h> // For time_t

// --- Structure for file access and modification times ---
struct utimbuf {
    time_t actime;  // Access time
    time_t modtime; // Modification time
};

// --- Function Prototypes ---
int utime(const char *filename, const struct utimbuf *times);

#endif // _UTIME_H