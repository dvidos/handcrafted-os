#ifndef _ASSERT_H
#define _ASSERT_H

#ifdef NDEBUG
#define assert(expression) ((void)0)
#else
// A simple assert implementation. In a real system, this might involve
// printing to stderr and then aborting or similar.
#define assert(expression) \
    ((expression) ? (void)0 : __assert_fail(#expression, __FILE__, __LINE__, __func__))

// Function called by assert on failure. This would typically be provided by libc.
void __assert_fail(const char *expression, const char *file, unsigned int line, const char *function);

#endif // NDEBUG

#endif // _ASSERT_H