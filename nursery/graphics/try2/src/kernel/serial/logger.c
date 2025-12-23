#include <stdarg.h>
#include "../memory/malloc.h"
#include "../memory/sprintf.h"
#include "logger.h"
#include "serial.h"

static logger_level _curr_log_level = LOG_LEVEL_ERROR;
static unsigned char *_log_buffer;
static int _log_buffer_size;

// ----------------------------------------------

void initialize_logger(logger_level initial_level) {
    initialize_serial_port();

    _curr_log_level = initial_level;
    _log_buffer_size = 1024;
    _log_buffer = kmalloc(_log_buffer_size);
}

logger_level logger_get_level() {
    return _curr_log_level;
}

void logger_set_level(logger_level new_level) {
    _curr_log_level = new_level;
}

static void log_something(const char *type, const char *fmt, va_list args) {
    // serial port for now, serial/memory/file/etc appenders later
    sprintfn(_log_buffer, _log_buffer_size, "%s: ", type); // could add timestamp
    serial_print_str(_log_buffer);
    vsprintfn(_log_buffer, _log_buffer_size, fmt, args);
    serial_print_str(_log_buffer);
    serial_print_str("\r\n");
}

static void _log_debug(const char *fmt, ...) {
    if (_curr_log_level > LOG_LEVEL_DEBUG) return;
    va_list list;
    va_start(list, fmt);
    log_something("DEBUG", fmt, list);
    va_end(list);
}

static void _log_info(const char *fmt, ...) {
    if (_curr_log_level > LOG_LEVEL_INFO) return;
    va_list list;
    va_start(list, fmt);
    log_something("INFO", fmt, list);
    va_end(list);
}

static void _log_warn(const char *fmt, ...) {
    if (_curr_log_level > LOG_LEVEL_WARN) return;
    va_list list;
    va_start(list, fmt);
    log_something("WARN", fmt, list);
    va_end(list);
}

static void _log_error(const char *fmt, ...) {
    if (_curr_log_level > LOG_LEVEL_ERROR) return;
    va_list list;
    va_start(list, fmt);
    log_something("ERROR", fmt, list);
    va_end(list);
}

static void _log_panic(const char *fmt, ...) {
    if (_curr_log_level > LOG_LEVEL_PANIC) return;
    va_list list;
    va_start(list, fmt);
    log_something("PANIC", fmt, list);
    va_end(list);
}

logger_methods log = {
    .debug = _log_debug,
    .info = _log_info,
    .warn = _log_warn,
    .error = _log_error,
    .panic = _log_panic
};
