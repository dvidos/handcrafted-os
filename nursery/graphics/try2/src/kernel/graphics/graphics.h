#pragma once
#include "color.h"
#include "font8x16.h"


// graphics_() family of functions work on root framebuffer directly.
void graphics_initialize(void *fb_address, int width, int height, int pitch, int bpp);
void graphics_fill(color c);
void graphics_rect(int x, int y, int width, int height, color clr);
void graphics_demo(int x, int y, int width, int height);
int  graphics_draw_8x16_text(int x, int baseline_y, const char *text, font8x16 *font, color clr);
int  graphics_draw_8x16_demo(int x, int baseline_y, font8x16 *font, color clr);

// gb_() family of functions work on graphics buffers
