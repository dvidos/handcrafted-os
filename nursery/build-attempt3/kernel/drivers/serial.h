#ifndef _SERIAL_H
#define _SERIAL_H

int init_serial_port();
void serial_write(char *str);
void serial_panic_writer(const char *str);
void serial_log_appender(void *context, const char *str);

#endif
