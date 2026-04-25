#ifndef _SERIAL_H
#define _SERIAL_H

// int init_serial_port();
// void serial_write(char *str);
// void serial_panic_writer(const char *str);
// void serial_log_appender(void *context, const char *str);




int init_serial_port(int port_idx);
void serial_putchar(int port_idx, char a);
void serial_write(int port_idx, const char *str);
void serial_enqueue_char(int port_idx, uint8_t c);
int serial_dequeue_char(int port_idx);
void serial_interrupt_handler(int port_idx);
char serial_wait_getc(int port_idx);
void serial_wait_gets(int port_idx, char *buf, int limit);
void serial_panic_writer(const char *str);
void serial_log_appender(void *context, const char *str);




    #endif
