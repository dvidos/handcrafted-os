#ifndef _HCOS_SYSLOG_H
#define _HCOS_SYSLOG_H

#include "../inttypes.h"



// these should mirror kernel's ones
#define SYSLOG_NONE      0
#define SYSLOG_CRITICAL  1
#define SYSLOG_ERROR     2
#define SYSLOG_WARNING   3
#define SYSLOG_INFO      4
#define SYSLOG_DEBUG     5
#define SYSLOG_TRACE     6

#define syslog_critical(...)            syslog(SYSLOG_CRITICAL, __VA_ARGS__)
#define syslog_error(...)               syslog(SYSLOG_ERROR, __VA_ARGS__)
#define syslog_warn(...)                syslog(SYSLOG_WARNING, __VA_ARGS__)
#define syslog_info(...)                syslog(SYSLOG_INFO, __VA_ARGS__)
#define syslog_debug(...)               syslog(SYSLOG_DEBUG, __VA_ARGS__)
#define syslog_trace(...)               syslog(SYSLOG_TRACE, __VA_ARGS__)
#define syslog_hex_debug(addr, len, start)   syslog_hex_dump(SYSLOG_DEBUG, addr, len, start)

void syslog(int level, char *, ...);
void syslog_hex_dump(int level, void *address, uint32_t length, uint32_t starting_num);




#endif // _HCOS_SYSLOG_H