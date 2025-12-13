#pragma once
#include "color.h"


// graphics_() family of functions work on root framebuffer directly.
void graphics_initialize(void *fb_address, int width, int height, int pitch, int bpp);
void graphics_fill(color c);
void graphics_demo(int x, int y, int width, int height);


// gb_() family of functions work on graphics buffers
