#ifndef _KLOG_H
#define _KLOG_H

#include <ctypes.h>
#include <devices/tty.h>



typedef enum log_level {
    LOGLEV_NONE = 0,
    LOGLEV_CRIT = 1,
    LOGLEV_ERROR = 2,
    LOGLEV_WARN = 3,
    LOGLEV_INFO = 4,
    LOGLEV_DEBUG = 5,
    LOGLEV_TRACE = 6
} log_level_t;

typedef enum log_appender {
    LOGAPP_MEMORY = 0,
    LOGAPP_SCREEN = 1,
    LOGAPP_SERIAL = 2,
    LOGAPP_FILE = 3,
    LOGAPP_TTY = 4,
} log_appender_t;
#define LOGAPP_SIZE 5

void init_klog();
void klog_appender_level(log_appender_t appender, log_level_t level);
void klog_default_module_level(log_level_t level);
void klog_module_level(char *module_name, log_level_t level);
void klog_set_tty(tty_t *tty);

void klog_append(const char *module_name, log_level_t level, const char *format, ...);
void klog_append_hex(const char *module_name, log_level_t level, uint8_t *buffer, size_t length, uint32_t start_address);
void klog_user_syslog(int level, char *buffer);


#define LOG_WITH_MODULE_NAMES
#ifdef LOG_WITH_MODULE_NAMES

    #define MODULE(module_name)     static char *__module_name = module_name;

    #define klog_crit(...)   klog_append(__module_name, LOGLEV_CRIT,  __VA_ARGS__)
    #define klog_error(...)  klog_append(__module_name, LOGLEV_ERROR, __VA_ARGS__)
    #define klog_warn(...)   klog_append(__module_name, LOGLEV_WARN,  __VA_ARGS__)
    #define klog_info(...)   klog_append(__module_name, LOGLEV_INFO,  __VA_ARGS__)
    #define klog_debug(...)  klog_append(__module_name, LOGLEV_DEBUG, __VA_ARGS__)
    #define klog_trace(...)  klog_append(__module_name, LOGLEV_TRACE, __VA_ARGS__)
    #define klog_debug_hex(buff,len,start)  klog_append_hex(__module_name, LOGLEV_DEBUG, buff, len, start)

#else

    #define MODULE(module_name)

    #define klog_crit(...)   klog_append(NULL, LOGLEV_CRIT,  __VA_ARGS__)
    #define klog_error(...)  klog_append(NULL, LOGLEV_ERROR, __VA_ARGS__)
    #define klog_warn(...)   klog_append(NULL, LOGLEV_WARN,  __VA_ARGS__)
    #define klog_info(...)   klog_append(NULL, LOGLEV_INFO,  __VA_ARGS__)
    #define klog_debug(...)  klog_append(NULL, LOGLEV_DEBUG, __VA_ARGS__)
    #define klog_trace(...)  klog_append(NULL, LOGLEV_TRACE, __VA_ARGS__)
    #define klog_debug_hex(buff,len,start)  klog_append_hex(NULL, LOGLEV_DEBUG, buff, len, start)

#endif




#endif