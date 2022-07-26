#ifndef _STDLIB_H
#define _STDLIB_H

#include <ctypes.h>



// these should mirror kernel's ones
#define SYSLOG_NONE      0
#define SYSLOG_CRITICAL  1
#define SYSLOG_ERROR     2
#define SYSLOG_WARNING   3
#define SYSLOG_INFO      4
#define SYSLOG_DEBUG     5
#define SYSLOG_TRACE     6

#define syslog_critical(fmt, ...)            syslog(SYSLOG_CRITICAL, fmt, __VA_ARGS__)
#define syslog_error(fmt, ...)               syslog(SYSLOG_ERROR, fmt, __VA_ARGS__)
#define syslog_warn(fmt, ...)                syslog(SYSLOG_WARNING, fmt, __VA_ARGS__)
#define syslog_info(fmt, ...)                syslog(SYSLOG_INFO, fmt, __VA_ARGS__)
#define syslog_debug(fmt, ...)               syslog(SYSLOG_DEBUG, fmt, __VA_ARGS__)
#define syslog_trace(fmt, ...)               syslog(SYSLOG_TRACE, fmt, __VA_ARGS__)
#define syslog_hex_debug(addr, len, start)   syslog_hex_dump(SYSLOG_DEBUG, addr, len, start)

void syslog(int level, char *, ...);
void syslog_hex_dump(int level, void *address, uint32_t length, uint32_t starting_num);



void exit(uint8_t exit_code);



#define DEBUG_HEAP_OPS  1

#ifdef DEBUG_HEAP_OPS

    #define malloc(size)          __malloc(size, #size, __FILE__, __LINE__)
    #define heap_verify()         __heap_verify(__FILE__, __LINE__)
    
    void __heap_verify(char *file, int line);
#else
    #define malloc(size)          __malloc(size, NULL, NULL, 0)
    #define heap_verify()         ((void)0)
#endif

void *__malloc(size_t size, char *expl, char *file, uint16_t line);
void free(void *ptr);
void *sbrk(int size_diff);
uint32_t heap_total_size();
uint32_t heap_free_size();
void heap_dump();





#endif
