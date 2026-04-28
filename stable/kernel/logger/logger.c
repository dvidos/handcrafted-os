#include "../include/va_list.h"
#include "../utils/panic.h"
#include "../klib/string.h"
#include "../klib/strbuff.h"
#include "../drivers/serial.h"
#include "../drivers/screen.h"
#include "../drivers/timer.h"
#include "../logger/logger.h"
#include "mem_log.h"

static char *level_captions[] = {
    "NONE ",
    "CRIT ",
    "ERROR",
    "WARN ",
    "INFO ",
    "DEBUG",
    "TRACE"
};



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
        logger_append("LOGGER", NULL, 0, NULL, 0, LOG_LEVEL_WARN, "Log appenders array full, increase array size");
        return;
    }

    appenders[appender_count].write = write;
    appenders[appender_count].context = context;
    appenders[appender_count].level = level;

    // since we added this, echo the mem_log, if any
    write(context, mem_log_get_contents());
    
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
    appender_count = 0;
    _logger_global_minimum_log_level = LOG_LEVEL_WARN;
}

void logger_set_global_minimum_log_level(log_level_t level) {
    _logger_global_minimum_log_level = level;
}

// --------------------------------------------------------------------------------------------------

static void _append_one_appender(const char *timing, const char *module_name, const char *file, unsigned line, const char *proc_name, pid_t pid, log_level_t level, const char *prompt, const char *message, appender_t *app) {
    if (!app->write)
        return;
    
    char buffer[128] = {0,};

    app->write(app->context, timing);
    app->write(app->context, " ");

    if (module_name != NULL) {
        strcpy(buffer, module_name);
    }
    app->write(app->context, buffer);
    for (int i = 0; i < 10 - strlen(module_name); i++) app->write(app->context, " ");

    if (file != NULL && line != 0) {
        char *sep = strrchr(file, '/');
        sprintfn(buffer, sizeof(buffer), "%s:%u ", sep == NULL ? file : sep + 1, line);
    } else if (proc_name != NULL && pid != 0) {
        sprintfn(buffer, sizeof(buffer), "%s[%u] ", proc_name, pid);
    } else {
        strcpy(buffer, "");
    }
    app->write(app->context, buffer);
    for (int i = 0; i < 18 - strlen(buffer); i++) app->write(app->context, " ");

    app->write(app->context, level_captions[level]);
    app->write(app->context, "  ");

    if (prompt != NULL && prompt[0] != 0) {
        app->write(app->context, prompt);
        app->write(app->context, " ");
    }

    app->write(app->context, message);
    app->write(app->context, "\n");
}

static void _append_all_appenders(const char *module_name, const char *file, unsigned line, const char *proc_name, pid_t pid, log_level_t level, const char *prompt, const char *message) {
    char timing[64];
    uint32_t msecs = (uint32_t)timer_get_uptime_msecs();
    sprintfn(timing, sizeof(timing), "%u.%03u", msecs / 1000, msecs % 1000);

    for (int i = 0; i < appender_count; i++) {
        appender_t *app = &appenders[i];
        if (app->write == 0)    continue;
        _append_one_appender(timing, module_name, file, line, proc_name, pid, level, prompt, message, app);
    }
}

// ----------------------------------------------------------------------

void logger_append(const char *module_name, const char *file, unsigned line, const char *proc_name, pid_t pid, log_level_t level, const char *format, ...) {
    if (format == NULL || strlen(format) == 0)
        return;

    va_list args;
    char message[256];
    va_start(args, format);
    vsprintfn(message, sizeof(message), format, args);
    va_end(args);

    _append_all_appenders(module_name, file, line, proc_name, pid, level, NULL, message);
}


struct _log_stream_printf_context {
    const char *timing;
    const char *module_name;
    const char *file;
    unsigned line;
    const char *proc_name;
    pid_t pid;
    log_level_t level;
    const char *prompt;
};

static void _log_stream_printf(log_write_stream_t *stream, const char *format, ...) {
    struct _log_stream_printf_context *ctx = (struct _log_stream_printf_context *)stream->context;

    va_list args;
    char message[256];
    va_start(args, format);
    vsprintfn(message, sizeof(message), format, args);
    va_end(args);

    _append_all_appenders(ctx->module_name, ctx->file, ctx->line, ctx->proc_name, ctx->pid, ctx->level, ctx->prompt, message);
}

static void _log_stream_print_fmt(log_write_stream_t *stream, char *prefix, log_formatter_t *formatter, ...) {
    struct _log_stream_printf_context *ctx = (struct _log_stream_printf_context *)stream->context;

    const char *original_prompt = ctx->prompt;
    if (prefix != NULL && prefix[0] != 0) {
        char composite_prompt[256] = {0};
        strncat(composite_prompt, ctx->prompt, sizeof(composite_prompt) - 1);
        strncat(composite_prompt, " ", sizeof(composite_prompt) - 1);
        strncat(composite_prompt, prefix, sizeof(composite_prompt) - 1);
        ctx->prompt = composite_prompt;
    }

    va_list args;
    va_start(args, formatter);
    formatter(stream, args);
    va_end(args);
    
    ctx->prompt = original_prompt;
}

void logger_append_using_formatter(const char *module_name, const char *file, unsigned line, const char *proc_name, pid_t pid, log_level_t level, const char *prompt, log_formatter_t *formatter, ...) {

    if (formatter == NULL)
        return;

    char timing[64];
    uint32_t msecs = (uint32_t)timer_get_uptime_msecs();
    sprintfn(timing, sizeof(timing), "%u.%03u", msecs / 1000, msecs % 1000);

    struct _log_stream_printf_context stream_context = {
        .level = level,
        .module_name = module_name,
        .file = file,
        .line = line,
        .proc_name = proc_name,
        .pid = pid,
        .timing = timing,
        .prompt = prompt
    };

    log_write_stream_t stream = {
        .printf    = _log_stream_printf,
        .print_fmt = _log_stream_print_fmt,
        .context = &stream_context,
    };

    va_list args;
    va_start(args, formatter);
    formatter(&stream, args);
    va_end(args);
}

static inline char is_printable(char c) {
    return (c >= ' ' && 'c' <= '~' ? c : '.');
}

void logger_append_hex(const char *module_name, const char *file, unsigned line, const char *proc_name, pid_t pid, log_level_t level, const uint8_t *buffer, size_t length, uint32_t start_address) {
    char last_row[16];
    bool have_last_row = false;
    bool star_given = false;

    if (length == 0)
        return;

    
    while (length > 0) {
        if (have_last_row && memcmp(buffer, last_row, 16) == 0) {
            if (!star_given) {
                logger_append(module_name, file, line, proc_name, pid, level, "*");
                star_given = true;
            }

            buffer += 16;
            length -= length > 16 ? 16 : length;
            start_address += 16;
            continue;
        }

        logger_append(module_name, file, line, proc_name, pid, level,
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

        memcpy(last_row, buffer, 16);
        have_last_row = true;
        star_given = false;

        buffer += 16;
        length -= length > 16 ? 16 : length;
        start_address += 16;
    }
}
