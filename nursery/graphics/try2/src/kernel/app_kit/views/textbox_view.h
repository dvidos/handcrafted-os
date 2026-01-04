#pragma once
#include "../view.h"
#include "../../memory/malloc.h"

typedef struct {
    view_t base;
    char buffer[256];
} textbox_view;



textbox_view *new_textbox_view();
void textbox_view_set_text(textbox_view *t, const char *text);


