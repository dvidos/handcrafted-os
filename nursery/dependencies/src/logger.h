#pragma once
#include <stdarg.h>
#include "mem_allocator.h"
#include "log_appender.h"


typedef struct logger logger;
typedef struct logger_interface logger_interface;

struct logger_interface {
    void (*critical)(logger_interface *l, const char *fmt, ...);
    void (*error)(logger_interface *l, const char *fmt, ...);
    void (*warn)(logger_interface *l, const char *fmt, ...);
    void (*info)(logger_interface *l, const char *fmt, ...);
    void (*debug)(logger_interface *l, const char *fmt, ...);
    void (*trace)(logger_interface *l, const char *fmt, ...);
};

struct logger {
    logger_interface *(*get_logger_interface)(logger *l);

    const char *(*get_name)(logger *l);
    void (*set_level)(logger *l, log_level level);
    log_level (*get_level)(logger *l);
    void (*add_appender)(logger *l, log_appender *appender);
    void (*remove_appender)(logger *l, log_appender *appender);
    void (*destroy)(logger *l);
};

logger *create_logger(mem_allocator *allocator, const char *name);
