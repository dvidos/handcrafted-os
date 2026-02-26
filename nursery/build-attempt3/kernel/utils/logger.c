#include "../include/va_list.h"
#include "../utils/panic.h"
#include "../klib/string.h"
#include "../klib/strbuff.h"
#include "../drivers/serial.h"
#include "../drivers/screen.h"
#include "../drivers/timer.h"
#include "../utils/logger.h"
#include "../multitask/process.h"

static char *level_captions[] = {
    "NONE ",
    "CRIT ",
    "ERROR",
    "WARN ",
    "INFO ",
    "DEBUG",
    "TRACE"
};

static struct {
    char buffer[4096];
    int len;
} memlog;
static void memlog_write(const char *str);



#define MAX_APPENDERS   8
typedef struct appender {
    log_level_t level;
    log_appender_func *write;
    void *context;
} appender_t;

static appender_t appenders[MAX_APPENDERS];
static int appender_count;
log_level_t _logger_global_minimum_log_level;

// -------------------------------------------------------------------------

void logger_add_appender(log_appender_func *write, void *context, log_level_t level) {
    if (appender_count >= MAX_APPENDERS) {
        logger_append("LOGGER", LOG_LEVEL_WARN, "Log appenders array full, increase array size");
        return;
    }

    appenders[appender_count].write = write;
    appenders[appender_count].context = context;
    appenders[appender_count].level = level;

    // since we added this, echo the memlog, if any
    if (memlog.len > 0) {
        write(context, "", "", "", memlog.buffer, true);
        memlog.len = 0;
    }
    
    appender_count++;
}

void logger_remove_appender(log_appender_func *write, void *context) {
    for (int i = 0; i < MAX_APPENDERS; i++) {
        if (appenders[i].write == write && appenders[i].context == context) {
            appenders[i].write = NULL;
            appenders[i].context = NULL;
            break;
        }
    }
}

void init_logger() {
    memset(memlog.buffer, '-', sizeof(memlog.buffer));
    memlog.len = 0;
    
    appender_count = 0;
    _logger_global_minimum_log_level = LOG_LEVEL_WARN;
}

void logger_set_global_minimum_log_level(log_level_t level) {
    _logger_global_minimum_log_level = level;
}

void logger_append(const char *module_name, log_level_t level, const char *format, ...) {
    if (format == NULL || strlen(format) == 0)
        return;

    char timing[64];
    uint32_t msecs = (uint32_t)timer_get_uptime_msecs();
    sprintfn(timing, sizeof(timing), "%u.%03u", msecs / 1000, msecs % 1000);

    va_list args;
    char message[256];
    va_start(args, format);
    vsprintfn(message, sizeof(message), format, args);
    va_end(args);

    if (appender_count < 2) {
        memlog_write(timing);
        memlog_write(" ");
        memlog_write(module_name);
        int padding = 10 - strlen(module_name);
        while (padding-- > 0) memlog_write(" ");
        memlog_write(" ");
        memlog_write(level_captions[level]);
        memlog_write(" ");
        memlog_write(message);
        memlog_write("\n");
    }

    for (int i = 0; i < appender_count; i++) {
        appender_t *app = &appenders[i];
        if (app->write == 0)    continue;
        if (level > app->level) continue; // so, the appender MUST support this level... hmm...
        app->write(app->context, timing, module_name, level_captions[level], message, false);
    }
}

static inline char is_printable(char c) {
    return (c >= ' ' && 'c' <= '~' ? c : '.');
}

void logger_append_hex(const char *module_name, log_level_t level,  uint8_t *buffer, size_t length, uint32_t start_address) {
    if (length == 0)
        return;

    while (length > 0) {
        // using xxd's format, seems nice
        logger_append(module_name, level,
            "%08x: %02x %02x %02x %02x %02x %02x %02x %02x  %02x %02x %02x %02x %02x %02x %02x %02x  %c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c",
            start_address,
            buffer[0], buffer[1], buffer[2], buffer[3], 
            buffer[4], buffer[5], buffer[6], buffer[7],
            buffer[8], buffer[9], buffer[10], buffer[11], 
            buffer[12], buffer[13], buffer[14], buffer[15],
            is_printable(buffer[0]), is_printable(buffer[1]), is_printable(buffer[2]), is_printable(buffer[3]),
            is_printable(buffer[4]), is_printable(buffer[5]), is_printable(buffer[6]), is_printable(buffer[7]),
            is_printable(buffer[8]), is_printable(buffer[9]), is_printable(buffer[10]), is_printable(buffer[11]),
            is_printable(buffer[12]), is_printable(buffer[13]), is_printable(buffer[14]), is_printable(buffer[15])
        );
        buffer += 16;
        length -= length > 16 ? 16 : length;
        start_address += 16;
    }
}



static void memlog_write(const char *str) {
    int slen = strlen(str) + 1; // include the zero terminator

    // if it does not fit, make just enough room for it
    if (memlog.len + slen >= (int)sizeof(memlog.buffer)) {
        memmove(memlog.buffer, memlog.buffer + slen, sizeof(memlog.buffer) - slen);
        memlog.len -= slen;
    }

    // now copy it.
    memcpy(&memlog.buffer[memlog.len], str, slen);
    memlog.len = strlen(memlog.buffer);
}
