#ifndef _LOGGER_H
#define _LOGGER_H

#include <ctypes.h>

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

typedef void (log_appender_func)(void *context, const char *timing, const char *module_name, const char *level, const char *message, bool raw_dump);
void logger_add_appender(log_appender_func *appender, void *context, log_level_t level);
void logger_remove_appender(log_appender_func *appender, void *context);

void logger_append(const char *module_name, log_level_t level, const char *format, ...);
void logger_append_hex(const char *module_name, log_level_t level, uint8_t *buffer, size_t length, uint32_t start_address);




#define LOG_WITH_MODULE_NAMES
#ifdef LOG_WITH_MODULE_NAMES

    typedef struct { const char *name; log_level_t level; } module_log_cfg_t;
    #define MODULE(module_name, log_level)     static module_log_cfg_t __log_cfg = { .name = module_name, .level = log_level };

    #define log_critical(...)   do { if (_logger_global_minimum_log_level >= LOG_LEVEL_CRIT  || __log_cfg.level >= LOG_LEVEL_CRIT)  logger_append(__log_cfg.name, LOG_LEVEL_CRIT,  __VA_ARGS__); } while (0)
    #define log_error(...)      do { if (_logger_global_minimum_log_level >= LOG_LEVEL_ERROR || __log_cfg.level >= LOG_LEVEL_ERROR) logger_append(__log_cfg.name, LOG_LEVEL_ERROR, __VA_ARGS__); } while (0)
    #define log_warn(...)       do { if (_logger_global_minimum_log_level >= LOG_LEVEL_WARN  || __log_cfg.level >= LOG_LEVEL_WARN)  logger_append(__log_cfg.name, LOG_LEVEL_WARN,  __VA_ARGS__); } while (0)
    #define log_info(...)       do { if (_logger_global_minimum_log_level >= LOG_LEVEL_INFO  || __log_cfg.level >= LOG_LEVEL_INFO)  logger_append(__log_cfg.name, LOG_LEVEL_INFO,  __VA_ARGS__); } while (0)
    #define log_debug(...)      do { if (_logger_global_minimum_log_level >= LOG_LEVEL_DEBUG || __log_cfg.level >= LOG_LEVEL_DEBUG) logger_append(__log_cfg.name, LOG_LEVEL_DEBUG, __VA_ARGS__); } while (0)
    #define log_trace(...)      do { if (_logger_global_minimum_log_level >= LOG_LEVEL_TRACE || __log_cfg.level >= LOG_LEVEL_TRACE) logger_append(__log_cfg.name, LOG_LEVEL_TRACE, __VA_ARGS__); } while (0)
    #define log_debug_hex(buff,len,start)  do { if (_logger_global_minimum_log_level >= LOG_LEVEL_DEBUG || __log_cfg.level >= LOG_LEVEL_DEBUG) logger_append_hex(__log_cfg.name, LOG_LEVEL_DEBUG, buff, len, start); } while (0)

#else

    #define MODULE(module_name, log_level)

    #define log_critical(...)   logger_append(NULL, LOG_LEVEL_CRIT,  __VA_ARGS__)
    #define log_error(...)      logger_append(NULL, LOG_LEVEL_ERROR, __VA_ARGS__)
    #define log_warn(...)       logger_append(NULL, LOG_LEVEL_WARN,  __VA_ARGS__)
    #define log_info(...)       logger_append(NULL, LOG_LEVEL_INFO,  __VA_ARGS__)
    #define log_debug(...)      logger_append(NULL, LOG_LEVEL_DEBUG, __VA_ARGS__)
    #define log_trace(...)      logger_append(NULL, LOG_LEVEL_TRACE, __VA_ARGS__)
    #define log_debug_hex(buff,len,start)  logger_append_hex(NULL, LOG_LEVEL_DEBUG, buff, len, start)

#endif




#endif // LOGGER_H
