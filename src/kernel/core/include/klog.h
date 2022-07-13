#ifndef _KLOG_H
#define _KLOG_H

#include <ctypes.h>
#include <devices/tty.h>

typedef enum log_level {
    LOGLEV_NONE = 0,
    LOGLEV_CRITICAL = 1,
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
void klog_set_tty(tty_t *tty);

void klog_trace(const char *format, ...);
void klog_debug(const char *format, ...);
void klog_info(const char *format, ...);
void klog_warn(const char *format, ...);
void klog_error(const char *format, ...);
void klog_critical(const char *format, ...);

void klog_hex16_debug(uint8_t *buffer, size_t length, uint32_t start_address);



#endif