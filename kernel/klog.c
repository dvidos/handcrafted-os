#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include "string.h"
#include "serial.h"
#include "screen.h"
#include "timer.h"
#include "process.h"


static char log_buffer[1024] = {0,};
static int log_len = 0;
static struct {
    char serial_enabled: 1;
    char screen_enabled: 1;
} __attribute__((packed)) log_flags;


void init_klog() {
    memset(log_buffer, 0, sizeof(log_buffer));
    log_len = 0;
    memset(&log_flags, 0, sizeof(log_flags));
}

void klog(const char *format, ...) {
    if (!log_flags.screen_enabled && log_flags.serial_enabled) {
        // preamble
        sprintfn(
            log_buffer + strlen(log_buffer),
            sizeof(log_buffer) - strlen(log_buffer),
            "%u: ",
            (uint32_t)(timer_get_uptime_msecs() & 0xFFFFFFFF)
        );
    }

    int log_starting_point = strlen(log_buffer);
    if (sizeof(log_buffer) - log_starting_point <= 1)
        panic("No space on kernel log buffer");

    va_list args;
    va_start(args, format);
    vsprintfn(
        log_buffer + log_starting_point,
        sizeof(log_buffer) - log_starting_point,
        format,
        args
    );
    va_end(args);
    log_len = strlen(log_buffer);

    // if we do have aat least one output, we'll clear the buffer
    bool persisted = false;
    if (log_flags.screen_enabled) {
        // since screen is not persistent, do just this one
        screen_write(log_buffer + log_starting_point);
    }
    if (log_flags.serial_enabled) {
        // since we will clean this, dump from the start
        serial_write(log_buffer);
        persisted = true;
    }
    if (persisted) {
        log_buffer[0] = '\0';
        log_len = 0;
    }
}

void klog_serial_port(bool enable) {
    log_flags.serial_enabled = enable;
}

void klog_screen(bool enable) {
    log_flags.screen_enabled = enable;
}
