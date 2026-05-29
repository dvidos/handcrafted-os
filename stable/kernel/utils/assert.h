#include "panic.h"
#include "../../config.inc.h"  // for switch to enable assertions

// these are mainly for development
#ifdef ENABLE_ASSERTIONS
    #define ASSERT_FAILED(msg)   panic("Assert failed: %s at %s:%d", msg, __FILE__, __LINE__)
    #define ASSERT(x)            do { if (!(x)) ASSERT_FAILED(#x);  } while(0)
#else
    #define ASSERT_FAILED(msg)   (void)0
    #define ASSERT(x)            (void)0
#endif


// these are always compiled in
#define BUG(msg)             panic("Bug: %s at %s:%d", msg, __FILE__, __LINE__)
#define BUG_ON(x)            do { if (x) BUG(#x);  } while(0)
#define STATIC_ASSERT(x)     _Static_assert(x, #x)

