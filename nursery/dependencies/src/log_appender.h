#pragma once
#include "fundamentals.h"
#include "mem_allocator.h"

typedef enum log_level {
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_CRITICAL,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_TRACE,
} log_level;


typedef struct log_appender log_appender;

struct log_appender {
    void (*append)(log_appender *a, const char *line);
    const char *(*peek_buffer)(log_appender *a); // allows peeking in memory appenders
    void (*destroy)(log_appender *a);
};

// these subject to availability (e.g. HOSTED vs STANDALONE system)
log_appender *create_in_memory_log_appender(mem_allocator *allocator);
#ifdef STANDALONE
    log_appender *create_serial_port_log_appender(mem_allocator *allocator, int port_no, int baud, int data_bits, char parity, int stop_bits);
#endif
#ifdef HOSTED
    log_appender *create_stderr_log_appender();
    log_appender *create_text_file_log_appender(const char *filename);
#endif
