#include <ctypes.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include "metrics.h"

#ifdef __is_libc // only available in userland


#define MIN_ITERATIONS              1
#define MAX_ITERATIONS         262144  // 256K times
#define ITERATIONS_MULTIPLIER       4

#define MAX_TIMING_WINDOW        1024
#define MIN_TIMING_WINDOW           2
#define TIMING_WINDOW_DIVIDER       2


#define HIGH_DWORD(value)      (uint32_t)(((value) >> 32) & 0xFFFFFFFF)
#define LOW_DWORD(value)       (uint32_t)(((value) >>  0) & 0xFFFFFFFF)


// Warning, this function may take seconds to execute...
void take_exec_metrics(function *func, char *name, exec_metrics *metrics) {

    // find out how many times the function can run in a single second.
    // if the function is too fast, halve the time window
    uint64_t msecs_started, msecs_finished;
    uint32_t msecs_elapsed, time_window_msecs, iterations, remaining;
    bool good_measurement;

    // start with these. iterations will tripple, time window with halve
    iterations = MIN_ITERATIONS;
    time_window_msecs = MAX_TIMING_WINDOW;
    good_measurement = false;

    while (true) {
        msecs_started = 0;
        msecs_finished = 0;

        syslog_info("%s: Trying %d iterations, up to %d msecs...", name, iterations, time_window_msecs);

        remaining = iterations;
        uptime(&msecs_started);
        while (remaining--)
            func();
        uptime(&msecs_finished);
        msecs_elapsed = (uint32_t)(msecs_finished - msecs_started);

        // if we were slow enough to exhaust the timing window, stop
        if (msecs_elapsed > time_window_msecs) {
            good_measurement = true;
            break;
        }

        // otherwise, we ran the iterations under the timing window, 
        // try more iterations, but only if we have room
        if (iterations * ITERATIONS_MULTIPLIER <= MAX_ITERATIONS) {
            iterations *= ITERATIONS_MULTIPLIER;
            continue;
        }

        // otherwise, we cannot try more iterations, keep them,
        // try lowering the timing window, if, there's room
        if (time_window_msecs / TIMING_WINDOW_DIVIDER >= MIN_TIMING_WINDOW) {
            time_window_msecs /= TIMING_WINDOW_DIVIDER;
            continue;
        }

        // otherwise, we are too fast to count the iterations
        // and the timing window is so small we can't go lower
        // give up...
        break;
    }

    // keep some statistics
    metrics->name = name;
    metrics->func = func;
    metrics->iterations_count = iterations;
    metrics->iterations_msecs = msecs_elapsed;
    metrics->unreliable = !good_measurement;

    // shall we have enough bits to multiply with 1000 ???
    metrics->iterations_per_second = (iterations * 1000 / msecs_elapsed);

    // hoping integer division does not bite us
    metrics->msecs_per_iteration = (msecs_elapsed / iterations);
}



#endif // __is_libc