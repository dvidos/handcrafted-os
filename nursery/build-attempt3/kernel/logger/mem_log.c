#include "../include/ctypes.h"
#include "../klib/string.h"
#include "mem_log.h"

static struct {
    char buffer[4096];
    int len;
} memlog = {
    .buffer = {0},
    .len = 0,
};

void mem_log_appender(void *context, const char *timing, const char *module_name, const char *level, const char *message, bool raw_dump) {
    int slen = strlen(message) + 1; // include the zero terminator

    // if it does not fit, make just enough room for it
    if (memlog.len + slen >= (int)sizeof(memlog.buffer)) {
        memmove(memlog.buffer, memlog.buffer + slen, sizeof(memlog.buffer) - slen);
        memlog.len -= slen;
    }

    // now copy it.
    memcpy(&memlog.buffer[memlog.len], message, slen);
    memlog.len = strlen(memlog.buffer);
}

const char *mem_log_get_contents() {
    return memlog.buffer;
}
