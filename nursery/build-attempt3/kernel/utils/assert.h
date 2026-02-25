#include "panic.h"


// these are mainly for development
#ifndef NO_ASSERTS
    #define ASSERT_FAILED(msg)   panic("Assert failed: %s at %s:%d", msg, __FILE__, __LINE__)
    #define ASSERT(x)            do { if (!(x)) ASSERT_FAILED(#x);  } while(0)
    #define ASSERT_MSG(x, msg)   do { if (!(x)) ASSERT_FAILED(msg);  } while(0)
#else
    #define ASSERT_FAILED(msg)   0
    #define ASSERT(x)            0
    #define ASSERT_MSG(x, msg)   0
#endif


// these are always compiled in
#define BUG(msg)             panic("Bug: %s at %s:%d", msg, __FILE__, __LINE__)
#define BUG_ON(x)            do { if (x) BUG(#x);  } while(0)
#define STATIC_ASSERT(x)     _Static_assert(x, #x)

