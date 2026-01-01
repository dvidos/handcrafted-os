#pragma once
#include "../view.h"
#include "../../memory/malloc.h"

typedef struct text_view {
    view_t base;
    const char *text;
} text_view;

text_view *new_text_view(const char *text);