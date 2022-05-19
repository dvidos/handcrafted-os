#ifndef _KLOG_H
#define _KLOG_H


void init_klog();
void klog(const char *format, ...);
void klog_hex16(char *buffer, size_t length, uint32_t start_address);
void klog_serial_port(bool enable);
void klog_screen(bool enable);




#endif