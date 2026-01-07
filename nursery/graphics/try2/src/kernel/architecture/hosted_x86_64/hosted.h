#pragma once

/*
    To allow the kernel to run in a hosted environment in linux
*/

#include <stdint.h>

typedef enum {
    PLATFORM_EVENT_NONE,
    PLATFORM_EVENT_QUIT,
    PLATFORM_EVENT_KEY_DOWN,
    PLATFORM_EVENT_KEY_UP,
    PLATFORM_EVENT_MOUSE_MOVE,
    PLATFORM_EVENT_MOUSE_BUTTON,
} platform_event_type_t;

typedef struct {
    platform_event_type_t type;
    union {
        struct { int keycode; } key;
        struct { int x, y; } mouse_move;
        struct { int button; int down; } mouse_button;
    };
} platform_event_t;

void platform_init(int width, int height);
void platform_shutdown(void);

uint32_t *platform_framebuffer(void);
int platform_fb_width(void);
int platform_fb_height(void);

int platform_poll_event(platform_event_t *out);
void platform_present(void);
