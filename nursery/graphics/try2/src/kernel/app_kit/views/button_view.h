#pragma once
#include "../view.h"

typedef void (click_handling_func)(void *);

typedef struct button_view {
    view_t base;
    const char *label;
    click_handling_func *on_click;
    void *userdata;
    bool pressed;
    bool is_default;
    bool is_cancel;
} button_view;


button_view *new_button_view(const char *label, click_handling_func *on_click, void *userdata);
button_view *new_default_button_view(const char *label, click_handling_func *on_click, void *userdata);
button_view *new_cancel_button_view(const char *label, click_handling_func *on_click, void *userdata);

void button_view_perform_click(button_view *v);
