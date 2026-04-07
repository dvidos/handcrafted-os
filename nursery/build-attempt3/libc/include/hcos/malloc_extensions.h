#ifndef _HCOS_MALLOC_EXTENSIONS
#define _HCOS_MALLOC_EXTENSIONS

#include "../stddef.h"
#include "../inttypes.h"



#define HCOS_HEAP_EXTENSION
#ifdef HCOS_HEAP_EXTENSION
    #define malloc(size)          __malloc(size, #size, __FILE__, __LINE__)
    #define heap_verify()         __heap_verify(__FILE__, __LINE__)
    void __heap_verify(char *file, int line);
    void *__malloc(size_t size, char *explanation, char *file, uint16_t line);
#else
    #define malloc(size)          __malloc(size, NULL, NULL, 0)
    #define heap_verify()         ((void)0)
#endif




#endif // _HCOS_MALLOC_EXTENSIONS
