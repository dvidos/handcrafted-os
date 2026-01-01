#pragma once
#include "../view.h"
#include "../../memory/malloc.h"

typedef void (click_handling_func)(void *);

typedef struct button_view {
    view_t base;
    const char *label;
    click_handling_func *on_click;
    void *userdata;
    bool pressed;
} button_view;


button_view *new_button_view(const char *label, click_handling_func *on_click, void *userdata);