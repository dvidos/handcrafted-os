#ifndef _LOGGER_H
#define _LOGGER_H

#include <ctypes.h>

// TODO: make logger not depend on TTY, but on the appender abstraction
//       then make TTYs and files implement the abstraction and register with logger
#include "../devices/tty.h"



typedef enum log_level {
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_CRIT = 1,
    LOG_LEVEL_ERROR = 2,
    LOG_LEVEL_WARN = 3,
    LOG_LEVEL_INFO = 4,
    LOG_LEVEL_DEBUG = 5,
    LOG_LEVEL_TRACE = 6
} log_level_t;

typedef enum log_appender {
    LOG_APPENDER_MEMORY = 0,
    LOG_APPENDER_SCREEN = 1,
    LOG_APPENDER_SERIAL = 2,
    LOG_APPENDER_FILE = 3,
    LOG_APPENDER_TTY = 4,
} log_appender_t;
#define LOG_APPENDER_SIZE 5

void init_logger();
void logger_set_default_log_level(log_level_t level);
void logger_set_appender_log_level(log_appender_t appender, log_level_t level);
void logger_set_module_log_level(char *module_name, log_level_t level);
void logger_set_tty(tty_t *tty);

void logger_append(const char *module_name, log_level_t level, const char *format, ...);
void logger_append_hex(const char *module_name, log_level_t level, uint8_t *buffer, size_t length, uint32_t start_address);
void logger_append_user_syslog(int level, char *buffer);


#define LOG_WITH_MODULE_NAMES
#ifdef LOG_WITH_MODULE_NAMES

    #define MODULE(module_name)     static char *__module_name = module_name;

    #define log_critical(...)   logger_append(__module_name, LOG_LEVEL_CRIT,  __VA_ARGS__)
    #define log_error(...)      logger_append(__module_name, LOG_LEVEL_ERROR, __VA_ARGS__)
    #define log_warn(...)       logger_append(__module_name, LOG_LEVEL_WARN,  __VA_ARGS__)
    #define log_info(...)       logger_append(__module_name, LOG_LEVEL_INFO,  __VA_ARGS__)
    #define log_debug(...)      logger_append(__module_name, LOG_LEVEL_DEBUG, __VA_ARGS__)
    #define log_trace(...)      logger_append(__module_name, LOG_LEVEL_TRACE, __VA_ARGS__)
    #define log_debug_hex(buff,len,start)  logger_append_hex(__module_name, LOG_LEVEL_DEBUG, buff, len, start)

#else

    #define MODULE(module_name)

    #define log_critical(...)   logger_append(NULL, LOG_LEVEL_CRIT,  __VA_ARGS__)
    #define log_error(...)      logger_append(NULL, LOG_LEVEL_ERROR, __VA_ARGS__)
    #define log_warn(...)       logger_append(NULL, LOG_LEVEL_WARN,  __VA_ARGS__)
    #define log_info(...)       logger_append(NULL, LOG_LEVEL_INFO,  __VA_ARGS__)
    #define log_debug(...)      logger_append(NULL, LOG_LEVEL_DEBUG, __VA_ARGS__)
    #define log_trace(...)      logger_append(NULL, LOG_LEVEL_TRACE, __VA_ARGS__)
    #define log_debug_hex(buff,len,start)  logger_append_hex(NULL, LOG_LEVEL_DEBUG, buff, len, start)

#endif




#endif // LOGGER_H
