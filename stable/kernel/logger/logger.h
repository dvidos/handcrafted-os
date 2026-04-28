#ifndef _LOGGER_H
#define _LOGGER_H

#include "../../config.inc.h"
#include "../include/ctypes.h"
#include "../include/va_list.h"
#include "../klib/strerror.h"


typedef enum log_level {
    LOG_LEVEL_NONE  = 0,
    LOG_LEVEL_CRIT  = 1,
    LOG_LEVEL_ERROR = 2,
    LOG_LEVEL_WARN  = 3,
    LOG_LEVEL_INFO  = 4,
    LOG_LEVEL_DEBUG = 5,
    LOG_LEVEL_TRACE = 6
} log_level_t;

extern log_level_t _logger_global_minimum_log_level;

void init_logger();
void logger_set_global_minimum_log_level(log_level_t level);

// ----------------------------------------------

typedef void (log_appender_func)(void *context, const char *str);
void logger_add_appender(log_appender_func *appender, void *context, log_level_t level);
void logger_remove_appender(log_appender_func *appender, void *context);

// ----------------------------------------------

typedef struct log_stream_writer log_write_stream_t;
typedef void log_formatter_t(log_write_stream_t *stream, va_list args);

struct log_stream_writer {
    void (*printf)(log_write_stream_t *stream, const char *fmt, ...);
    void (*print_fmt)(log_write_stream_t *stream, char *prefix, log_formatter_t *fmt, ...);
    void *context;
};

// ----------------------------------------------

void logger_append(const char *module_name, const char *file, unsigned line, const char *proc_name, pid_t pid, log_level_t level, const char *format, ...);
void logger_append_using_formatter(const char *module_name, const char *file, unsigned line, const char *proc_name, pid_t pid, log_level_t level, const char *prompt, log_formatter_t *formatter, ...);
void logger_append_hex(const char *module_name, const char *file, unsigned line, const char *proc_name, pid_t pid, log_level_t level, const uint8_t *buffer, size_t length, uint32_t start_address);


typedef struct { const char *name; log_level_t level; } module_log_cfg_t;

// declare each file's module name and level
#define MODULE(module_name, log_level)     static module_log_cfg_t __module_log_configuration__ = { .name = module_name, .level = log_level };

// printf style logging
#define log_critical(...)         do { if (_logger_global_minimum_log_level >= LOG_LEVEL_CRIT  || __module_log_configuration__.level >= LOG_LEVEL_CRIT)  logger_append(__module_log_configuration__.name, __FILE__, __LINE__, NULL, 0, LOG_LEVEL_CRIT,  __VA_ARGS__); } while (0)
#define log_error(...)            do { if (_logger_global_minimum_log_level >= LOG_LEVEL_ERROR || __module_log_configuration__.level >= LOG_LEVEL_ERROR) logger_append(__module_log_configuration__.name, __FILE__, __LINE__, NULL, 0, LOG_LEVEL_ERROR, __VA_ARGS__); } while (0)
#define log_warn(...)             do { if (_logger_global_minimum_log_level >= LOG_LEVEL_WARN  || __module_log_configuration__.level >= LOG_LEVEL_WARN)  logger_append(__module_log_configuration__.name, __FILE__, __LINE__, NULL, 0, LOG_LEVEL_WARN,  __VA_ARGS__); } while (0)
#define log_info(...)             do { if (_logger_global_minimum_log_level >= LOG_LEVEL_INFO  || __module_log_configuration__.level >= LOG_LEVEL_INFO)  logger_append(__module_log_configuration__.name, __FILE__, __LINE__, NULL, 0, LOG_LEVEL_INFO,  __VA_ARGS__); } while (0)
#define log_debug(...)            do { if (_logger_global_minimum_log_level >= LOG_LEVEL_DEBUG || __module_log_configuration__.level >= LOG_LEVEL_DEBUG) logger_append(__module_log_configuration__.name, __FILE__, __LINE__, NULL, 0, LOG_LEVEL_DEBUG, __VA_ARGS__); } while (0)
#define log_trace(...)            do { if (_logger_global_minimum_log_level >= LOG_LEVEL_TRACE || __module_log_configuration__.level >= LOG_LEVEL_TRACE) logger_append(__module_log_configuration__.name, __FILE__, __LINE__, NULL, 0, LOG_LEVEL_TRACE, __VA_ARGS__); } while (0)
#define log_custom(level, ...)    do { if (_logger_global_minimum_log_level >= level           || __module_log_configuration__.level >= level)           logger_append(__module_log_configuration__.name, __FILE__, __LINE__, NULL, 0, level,           __VA_ARGS__); } while (0)

// provide a formatter, to format the value
#define log_error_fmt(formatter, prompt, value)      do { if (_logger_global_minimum_log_level >= LOG_LEVEL_ERROR || __module_log_configuration__.level >= LOG_LEVEL_ERROR) logger_append_using_formatter(__module_log_configuration__.name, __FILE__, __LINE__, NULL, 0, LOG_LEVEL_ERROR, prompt, formatter, value); } while (0)
#define log_info_fmt(formatter, prompt, value)       do { if (_logger_global_minimum_log_level >= LOG_LEVEL_INFO  || __module_log_configuration__.level >= LOG_LEVEL_INFO)  logger_append_using_formatter(__module_log_configuration__.name, __FILE__, __LINE__, NULL, 0, LOG_LEVEL_INFO,  prompt, formatter, value); } while (0)
#define log_debug_fmt(formatter, prompt, value)      do { if (_logger_global_minimum_log_level >= LOG_LEVEL_DEBUG || __module_log_configuration__.level >= LOG_LEVEL_DEBUG) logger_append_using_formatter(__module_log_configuration__.name, __FILE__, __LINE__, NULL, 0, LOG_LEVEL_DEBUG, prompt, formatter, value); } while (0)

// hex dumping
#define log_debug_hex(buff,len,start)  do { if (_logger_global_minimum_log_level >= LOG_LEVEL_DEBUG || __module_log_configuration__.level >= LOG_LEVEL_DEBUG) logger_append_hex(__module_log_configuration__.name, __FILE__, __LINE__, NULL, 0, LOG_LEVEL_DEBUG, buff, len, start); } while (0)




#if TRACE_RETURNS == 1
    #define traceable(err)     \
        ((err) == OK ? OK : (\
            logger_append(__module_log_configuration__.name, __FILE__, __LINE__, NULL, 0, LOG_LEVEL_WARN, \
                "%s() { return %s; (%d) }", \
                __FUNCTION__, strerror(err), err), \
            (err) \
        ))
#else 
    #define traceable(err)     (err)
#endif



#endif // LOGGER_H
