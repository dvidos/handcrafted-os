#pragma once 


typedef struct user_io user_io_t;

struct user_io {
    void (*printf)(user_io_t *ui, const char *fmt, ...);
    int (*getc)(user_io_t *ui);
    void *context;
};


user_io_t *get_screen_user_io(int port_idx);
user_io_t *get_serial_port_user_io(int port_idx);

