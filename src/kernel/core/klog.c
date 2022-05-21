#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include "string.h"
#include "drivers/serial.h"
#include "drivers/screen.h"
#include "drivers/timer.h"
#include "klog.h"


static char *level_captions[] = {
    "NONE",
    "CRITICAL",
    "ERROR",
    "WARN",
    "INFO",
    "DEBUG",
    "TRACE"
};

static struct {
    char buffer[2048];
    int len;
} memlog;

static struct {
    struct appender_info {
        log_level_t level;
        bool have_dumped_memory;
    } appenders[4];
} __attribute__((packed)) log_flags;



static void klog_append(log_level_t level, const char *format, va_list args);
static void memlog_write(char *str);
static void memory_log_append(log_level_t level, char *str);
static void screen_log_append(log_level_t level, char *str);
static void serial_log_append(log_level_t level, char *str);
static void file_log_append(log_level_t level, char *str);


void init_klog() {
    memset(&memlog, 0, sizeof(memlog));
    memset(&log_flags, 0, sizeof(log_flags));
}

void klog_appender_level(log_appender_t appender, log_level_t level) {
    bool prev_level = log_flags.appenders[appender].level;
    log_flags.appenders[appender].level = level;

    // maybe dump memory contents to this newly opened appender
    if (appender != LOGAPP_MEMORY &&
        prev_level == LOGLEV_NONE && 
        level > LOGLEV_NONE &&
        !log_flags.appenders[appender].have_dumped_memory
    ) {
        if (appender == LOGAPP_SERIAL)
            serial_log_append(LOGLEV_INFO, memlog.buffer);
        else if (appender == LOGAPP_FILE)
            file_log_append(LOGLEV_INFO, memlog.buffer);
        log_flags.appenders[appender].have_dumped_memory = true;
    }
}

void klog_trace(const char *format, ...) {
    va_list args;
    va_start(args, format);
    klog_append(LOGLEV_TRACE, format, args);
    va_end(args);
}

void klog_debug(const char *format, ...) {
    va_list args;
    va_start(args, format);
    klog_append(LOGLEV_DEBUG, format, args);
    va_end(args);
}

void klog_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    klog_append(LOGLEV_INFO, format, args);
    va_end(args);
}

void klog_warn(const char *format, ...) {
    va_list args;
    va_start(args, format);
    klog_append(LOGLEV_WARN, format, args);
    va_end(args);
}

void klog_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    klog_append(LOGLEV_ERROR, format, args);
    va_end(args);
}

void klog_critical(const char *format, ...) {
    va_list args;
    va_start(args, format);
    klog_append(LOGLEV_CRITICAL, format, args);
    va_end(args);
}

static void klog_append(log_level_t level, const char *format, va_list args) {
    char buffer[256];
    vsprintfn(buffer, sizeof(buffer), format, args);
    memory_log_append(level, buffer);
    screen_log_append(level, buffer);
    serial_log_append(level, buffer);
    file_log_append(level, buffer);
}


// write to memory log, losing older data if needed
// would go with a circular buffer, but too complex to code right now
static void memlog_write(char *str) {
    int slen = strlen(str) + 1; // include the zero terminator

    // if it does not fit, make just enough room for it
    if (memlog.len + slen >= (int)sizeof(memlog.buffer)) {
        memmove(memlog.buffer, &memlog.buffer[slen], sizeof(memlog.buffer) - slen);
        memlog.len -= slen;
    }

    // now copy it.
    memcpy(&memlog.buffer[memlog.len], str, slen);
}

static void memory_log_append(log_level_t level, char *str) {
    if (level > log_flags.appenders[LOGAPP_MEMORY].level)
        return;

    // first write preamble
    char buff[16];
    uint32_t msecs = (uint32_t)timer_get_uptime_msecs();
    sprintfn(buff, sizeof(buff), "%u.%03u [%s] ", msecs / 1000, msecs % 1000, level_captions[level]);
    memlog_write(buff);
    memlog_write(str);
    memlog_write("\n");
}

static void screen_log_append(log_level_t level, char *str) {
    if (level > log_flags.appenders[LOGAPP_SCREEN].level)
        return;

    uint8_t old_color = screen_get_color();
    if (level < LOGLEV_ERROR)
        screen_set_color(VGA_COLOR_LIGHT_RED);
    else if (level == LOGLEV_WARN)
        screen_set_color(VGA_COLOR_LIGHT_BROWN);    
    screen_write(str);
    screen_write("\n");
    screen_set_color(old_color);
}

static void serial_log_append(log_level_t level, char *str) {
    if (level > log_flags.appenders[LOGAPP_SERIAL].level)
        return;

    // first write preamble
    char buff[16];
    uint32_t msecs = (uint32_t)timer_get_uptime_msecs();
    sprintfn(buff, sizeof(buff), "%u.%03u [%s] ", msecs / 1000, msecs % 1000, level_captions[level]);
    serial_write(buff);
    serial_write(str);
    serial_write("\n");
}

static void file_log_append(log_level_t level, char *str) {
    ; // not implemented yet. hopefully /var/log/kernel.log
}

static inline char printable(char c) {
    return (c >= ' ' && 'c' <= '~' ? c : '.');
}

void klog_hex16_info(uint8_t *buffer, size_t length, uint32_t start_address) {
    while (length > 0) {
        // using xxd's format, seems nice
        klog_info("%08x: %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x  %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
            start_address,
            buffer[0], buffer[1], buffer[2], buffer[3], 
            buffer[4], buffer[5], buffer[6], buffer[7],
            buffer[8], buffer[9], buffer[10], buffer[11], 
            buffer[12], buffer[13], buffer[14], buffer[15],
            printable(buffer[0]), printable(buffer[1]), printable(buffer[2]), printable(buffer[3]),
            printable(buffer[4]), printable(buffer[5]), printable(buffer[6]), printable(buffer[7]),
            printable(buffer[8]), printable(buffer[9]), printable(buffer[10]), printable(buffer[11]),
            printable(buffer[12]), printable(buffer[13]), printable(buffer[14]), printable(buffer[15])
        );
        buffer += 16;
        length -= 16;
        start_address += 16;
    }
}

