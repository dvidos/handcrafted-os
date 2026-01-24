#include "logger.h"
#include "fundamentals.h"
#include "strings.h"


typedef struct logger_private_struct {
    // embedding structs and using offsetof(), container_of()
    logger logger;
    logger_interface interface;

    // private members here on
    mem_allocator *allocator;
    char *name;
    log_level level;

    #define MAX_APPENDERS  8
    log_appender *appenders[MAX_APPENDERS];

} logger_private_struct;


static const char *_log_level_name(log_level level) {
    switch (level) {
        case LOG_LEVEL_CRITICAL: return "CRIT";
        case LOG_LEVEL_ERROR:    return "ERROR";
        case LOG_LEVEL_WARNING:  return "WARN";
        case LOG_LEVEL_INFO:     return "INFO";
        case LOG_LEVEL_DEBUG:    return "DEBUG";
        case LOG_LEVEL_TRACE:    return "TRACE";
    }
    return "";
}

static void _logger_interface_message(logger_private_struct *d, log_level level, const char *fmt, va_list args) {
    // do something here
    
    char buffer[256];
    int line_len;

    #ifdef HOSTED
        #include <stdio.h>
        // could have timestamp here
        line_len +=  snprintf(buffer + line_len, sizeof(buffer) - line_len, "[%s] %s: ", d->name, _log_level_name(level)); 
        line_len += vsnprintf(buffer + line_len, sizeof(buffer) - line_len, fmt, args);
        line_len +=  snprintf(buffer + line_len, sizeof(buffer) - line_len, "\n"); 
    #elif STANDALONE
        // own implementation of vsprintf

    #endif

    for (int i = 0; i < MAX_APPENDERS && d->appenders[i] != NULL; i++) {
        log_appender *appender = d->appenders[i];
        appender->append(appender, buffer);
    }
}

static void _logger_interface_critical(logger_interface *l, const char *fmt, ...) {
    logger_private_struct *d = container_of(l, logger_private_struct, interface);

    if (d->level >= LOG_LEVEL_CRITICAL) {
        va_list args;
        va_start(args, fmt);
        _logger_interface_message(d, LOG_LEVEL_CRITICAL, fmt, args);
        va_end(args);
    }
}

static void _logger_interface_error(logger_interface *l, const char *fmt, ...) {
    logger_private_struct *d = container_of(l, logger_private_struct, interface);

    if (d->level >= LOG_LEVEL_ERROR) {
        va_list args;
        va_start(args, fmt);
        _logger_interface_message(d, LOG_LEVEL_ERROR, fmt, args);
        va_end(args);
    }
}

static void _logger_interface_warn(logger_interface *l, const char *fmt, ...) {
    logger_private_struct *d = container_of(l, logger_private_struct, interface);

    if (d->level >= LOG_LEVEL_WARNING) {
        va_list args;
        va_start(args, fmt);
        _logger_interface_message(d, LOG_LEVEL_WARNING, fmt, args);
        va_end(args);
    }
}

static void _logger_interface_info(logger_interface *l, const char *fmt, ...) {
    logger_private_struct *d = container_of(l, logger_private_struct, interface);

    if (d->level >= LOG_LEVEL_INFO) {
        va_list args;
        va_start(args, fmt);
        _logger_interface_message(d, LOG_LEVEL_INFO, fmt, args);
        va_end(args);
    }
}

static void _logger_interface_debug(logger_interface *l, const char *fmt, ...) {
    logger_private_struct *d = container_of(l, logger_private_struct, interface);

    if (d->level >= LOG_LEVEL_DEBUG) {
        va_list args;
        va_start(args, fmt);
        _logger_interface_message(d, LOG_LEVEL_DEBUG, fmt, args);
        va_end(args);
    }
}

static void _logger_interface_trace(logger_interface *l, const char *fmt, ...) {
    logger_private_struct *d = container_of(l, logger_private_struct, interface);

    if (d->level >= LOG_LEVEL_TRACE) {
        va_list args;
        va_start(args, fmt);
        _logger_interface_message(d, LOG_LEVEL_TRACE, fmt, args);
        va_end(args);
    }
}

static logger_interface *_logger_get_logger_interface(logger *l) {
    logger_private_struct *d = container_of(l, logger_private_struct, logger);
    return &d->interface;
}

static const char *_logger_get_name(logger *l) {
    logger_private_struct *d = container_of(l, logger_private_struct, logger);
    return d->name;
}    

static void _logger_set_level(logger *l, log_level level) {
    logger_private_struct *d = container_of(l, logger_private_struct, logger);
    d->level = level;
}

static log_level _logger_get_level(logger *l) {
    logger_private_struct *d = container_of(l, logger_private_struct, logger);
    return d->level;

}

static void _logger_add_appender(logger *l, log_appender *appender) {
    logger_private_struct *d = container_of(l, logger_private_struct, logger);
    for (int i = 0; i < MAX_APPENDERS; i++) {
        if (d->appenders[i] != NULL)
            continue;

        d->appenders[i] = appender;
        break;
    }
}

static void _logger_remove_appender(logger *l, log_appender *appender) {
    logger_private_struct *d = container_of(l, logger_private_struct, logger);
    for (int i = 0; i < MAX_APPENDERS; i++) {
        if (d->appenders[i] != appender)
            continue;

    
        // shift everything down
        for (int j = i; j < MAX_APPENDERS - 1; j++)
            d->appenders[j] = d->appenders[j + 1];
        // clear last appender
        d->appenders[MAX_APPENDERS - 1] = NULL;
        break;
    }
}

static void _logger_destroy(logger *l) {
    logger_private_struct *d = container_of(l, logger_private_struct, logger);
    mem_allocator *allocator = d->allocator;
    allocator->release(allocator, (void *)d->name);
    allocator->release(allocator, d);
}

logger *create_logger(mem_allocator *allocator, const char *name) {

    logger_private_struct *l = allocator->allocate(allocator, sizeof(logger_private_struct));
    l->allocator = allocator;
    l->name = allocator->allocate(allocator, strlen(name) + 1);
    strcpy(l->name, name);
    l->level = LOG_LEVEL_WARNING;
    memset(l->appenders, 0, sizeof(l->appenders));

    l->interface.critical          = _logger_interface_critical;
    l->interface.error             = _logger_interface_error;
    l->interface.warn              = _logger_interface_warn;
    l->interface.info              = _logger_interface_info;
    l->interface.debug             = _logger_interface_debug;
    l->interface.trace             = _logger_interface_trace;
    
    l->logger.get_logger_interface = _logger_get_logger_interface;
    l->logger.get_name             = _logger_get_name;
    l->logger.set_level            = _logger_set_level;
    l->logger.get_level            = _logger_get_level;
    l->logger.add_appender         = _logger_add_appender;
    l->logger.remove_appender      = _logger_remove_appender;
    l->logger.destroy              = _logger_destroy;

    return &l->logger;
}


