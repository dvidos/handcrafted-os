#include "libc_internal.h"



void __assert_fail(const char *expression, const char *file, unsigned int line, const char *function) {
    // 1. Print the detailed error to stderr
    fprintf(stderr, "%s:%u: %s: Assertion `%s' failed.\n", file, line, function, expression);

    // 2. Flush all streams to ensure the message is physically written
    fflush(NULL);

    // 3. Terminate abnormally
    abort();
}

