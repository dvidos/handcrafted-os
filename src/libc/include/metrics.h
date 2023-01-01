#ifndef _METRICS_H
#define _METRICS_H

#include <ctypes.h>

typedef void (function)();

typedef struct exec_metrics {
    function *func;
    char *name;
    uint32_t iterations_count;
    uint32_t iterations_msecs;
    uint32_t iterations_per_second;
    uint32_t msecs_per_iteration;
    bool     unreliable;
} exec_metrics;


// Warning! this function may take seconds to execute...
void take_exec_metrics(function *func, char *name, exec_metrics *metrics);




#endif
