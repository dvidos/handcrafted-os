#ifndef _SYS_TIME_H
#define _SYS_TIME_H

#include <sys/types.h> // For time_t

// --- Structure for time values (seconds and microseconds) ---
struct timeval {
    time_t tv_sec;  // Seconds
    long   tv_usec; // Microseconds
};

#endif // _SYS_TIME_H