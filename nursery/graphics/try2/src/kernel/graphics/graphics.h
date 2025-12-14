#pragma once
#include "color.h"


// graphics_() family of functions work on root framebuffer directly.
void graphics_initialize(void *fb_address, int width, int height, int pitch, int bpp);
void graphics_fill(color c);
void graphics_rect(int x, int y, int width, int height, color clr);
void graphics_demo(int x, int y, int width, int height);
int graphics_draw_character(int x, int baseline_y, char chr, color clr);
int graphics_draw_text(int x, int baseline_y, const char *text, color clr);

// gb_() family of functions work on graphics buffers
