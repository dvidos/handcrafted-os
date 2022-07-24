#include <ctypes.h>
#include <string.h>
#include <va_list.h>
#include <syscall.h>

#ifdef __is_libc



static char syslog_buffer[128+1];

void syslog(int level, char *format, ...) {

    va_list args;
    va_start(args, format);
    vsprintfn(syslog_buffer, sizeof(syslog_buffer), format, args);
    va_end(args);

    syscall(SYS_LOG_ENTRY, level, (int)syslog_buffer, 0, 0, 0);
}


void syslog_hex_dump(int level, void *address, uint32_t length, uint32_t starting_num) {
    syscall(SYS_LOG_HEX_DUMP, level, (int)address, (int)length, (int)starting_num, 0);
}



#endif // __is_libc
