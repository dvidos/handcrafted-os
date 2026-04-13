#include "../libc_internal.h"

void syslog(int level, char *format, ...) {
    char syslog_buffer[128+1];

    va_list args;
    va_start(args, format);
    vsnprintf(syslog_buffer, sizeof(syslog_buffer), format, args);
    va_end(args);

    syscall(SYS_LOG_ENTRY, level, (int)syslog_buffer, 0, 0, 0);
}


void syslog_hex_dump(int level, void *address, uint32_t length, uint32_t starting_num) {
    syscall(SYS_LOG_HEX_DUMP, level, (int)address, (int)length, (int)starting_num, 0);
}

void syslog_proc_dump(int level) {
    syscall(SYS_LOG_PROC_DUMP, level, 0, 0, 0, 0);
}


