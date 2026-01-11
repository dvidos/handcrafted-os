#pragma once
#include "../fundamentals.h"

typedef struct gui_init_params gui_init_params;
typedef struct gui_event gui_event;

struct gui_init_params {
    struct framebuffer {
        void *address;
        int width;
        int height;
        int pitch;
        int bpp;
    };
    void (*delay_ticks)(u32 ticks);
    u32 (*get_ticks)();
    void *(*malloc)(u32 size);
    void (*free)(void *ptr);
};


// dependencies: framebuffer, heap, timer, events, logger
void gui_initialize(gui_init_params *params);
void gui_handle_event(gui_event *ev);
void gui_timer_ticked();

// what does the kernel use? (who decides what goes where?)
void gui_create_surface();
void gui_create_window();
void gui_add_surface();
void gui_add_window();

// what do apps use?
void gui_create_text_view();
void gui_create_textbox_view();
void gui_create_button_view();


// ----------------------------------------------------------------------------

