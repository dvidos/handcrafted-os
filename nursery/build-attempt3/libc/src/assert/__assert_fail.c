#include "../libc_internal.h"

/**
 * @brief Handles assertion failures.
 *
 * This function is called by the `assert` macro when its `expression` evaluates to false.
 * In a typical libc implementation, this function would:
 * 1. Print an error message to `stderr` (or a kernel log) indicating the failed expression,
 *    the file and line number where the assertion occurred, and the function name.
 * 2. Optionally, perform a stack trace or dump core.
 * 3. Terminate the program abnormally, often by calling `abort()`, `_exit()`, or triggering
 *    a system-specific panic/halt mechanism.
 *
 * The implementation must ensure that it does not rely on complex library functions that
 * might themselves be in a broken state during an assertion failure. It should use basic
 * I/O operations and system calls if possible.
 *
 * @param expression The string representation of the failed assertion expression.
 * @param file The name of the source file where the assertion failed.
 * @param line The line number in the source file where the assertion failed.
 * @param function The name of the function where the assertion failed.
 */
void __assert_fail(const char *expression, const char *file, unsigned int line, const char *function) {
    syslog_critical("%s:%d, in function %s(), assertion '%s' failed", file, line, function, expression);
    // either freeze, or exit, either way we must not continue
    // freezing, hoping this will make the errror visible
    // exit(-127);
    for (;;)
        sleep(3);
}