#include "user_io.h"
#include "../klib/string.h"
#include "../drivers/serial.h"
#include "../drivers/screen.h"
#include "../drivers/kbd_drv.h"



static void user_io_screen_printf(user_io_t *ui, const char *fmt, ...) {
    char buff[256];

    va_list args;
    va_start(args, fmt);
    vsprintfn(buff, sizeof(buff), fmt, args);
    va_end(args);

    int idx = (int)ui->context;
    serial_write(idx, buff);
}

static int user_io_screen_getc(user_io_t *ui) {
    key_event_t event;
    while (true) {
        kbd_wait_get_event(&event);
        if (event.ascii == 0)
            continue;

        return (int)event.ascii;
    }
}

static void user_io_serial_port_printf(user_io_t *ui, const char *fmt, ...) {
    char buff[256];

    va_list args;
    va_start(args, fmt);
    vsprintfn(buff, sizeof(buff), fmt, args);
    va_end(args);

    int idx = (int)ui->context;
    serial_write(idx, buff);
}

static int user_io_serial_port_getc(user_io_t *ui) {
    int idx = (int)ui->context;
    return (int)serial_wait_getc(idx);
}


static user_io_t _screen_port_user_io = {
    .printf = user_io_screen_printf,
    .getc = user_io_screen_getc,
    .context = NULL
};

static user_io_t _serial_port_user_io = {
    .printf = user_io_serial_port_printf,
    .getc = user_io_serial_port_getc,
    .context = NULL
};

user_io_t *get_screen_user_io(int port_idx) {
    return &_screen_port_user_io;
}

user_io_t *get_serial_port_user_io(int port_idx) {
    _serial_port_user_io.context = (void *)port_idx;
    return &_serial_port_user_io;
}

