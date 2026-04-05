#ifndef _TIME_H
#define _TIME_H

#include <stddef.h> // For size_t
#include <sys/types.h> // For time_t

// --- Structure for calendar time (broken-down time) ---
struct tm {
    int tm_sec;   // Seconds [0, 60] (60 accounts for leap seconds)
    int tm_min;   // Minutes [0, 59]
    int tm_hour;  // Hours [0, 23]
    int tm_mday;  // Day of month [1, 31]
    int tm_mon;   // Month since January [0, 11]
    int tm_year;  // Years since 1900
    int tm_wday;  // Days since Sunday [0, 6]
    int tm_yday;  // Days since January 1 [0, 365]
    int tm_isdst; // Daylight Savings Time flag
};

// --- Structure for high-resolution time (seconds and nanoseconds) ---
struct timespec {
    time_t tv_sec;  // Seconds
    long   tv_nsec; // Nanoseconds
};

// --- Type for clock IDs ---
typedef int clockid_t;

// --- Function Prototypes (based on usage analysis) ---

// Time manipulation
time_t time(time_t *timer);
double difftime(time_t time1, time_t time0);
time_t mktime(struct tm *timeptr);

// High-resolution time
int clock_getres(clockid_t clk_id, struct timespec *res);
int clock_gettime(clockid_t clk_id, struct timespec *tp);
int nanosleep(const struct timespec *req, struct timespec *rem);

// Time conversion
char *asctime(const struct tm *timeptr);
char *ctime(const time_t *timer);
struct tm *gmtime(const time_t *timer);
struct tm *localtime(const time_t *timer);
size_t strftime(char *s, size_t maxsize, const char *format, const struct tm *timeptr);

#endif // _TIME_H