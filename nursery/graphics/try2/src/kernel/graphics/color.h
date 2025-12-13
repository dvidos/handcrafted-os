#pragma once
#include <stdint.h>

typedef uint32_t color; // first byte = opacity, FF=opaque, 00=transparect

#define RGB_A(clr)          (((clr) >> 24) & 0xFF)
#define RGB_R(clr)          (((clr) >> 16) & 0xFF)
#define RGB_G(clr)          (((clr) >>  8) & 0xFF)
#define RGB_B(clr)          (((clr) >>  0) & 0xFF)
#define RGBA(r, g, b, a)    (((a) & 0xFF) << 24) | (((r) & 0xFF) << 16) | (((g) & 0xFF) << 8) | ((b) & 0xFF);

color color_rgba(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);
color color_rgb(uint8_t red, uint8_t green, uint8_t blue);
color color_black();
color color_gray();
color color_white();
color color_transparent();

color color_between(color c1, color c2, float distance_factor); // factor in [0,1]
color color_darken(color c, float darkness_factor); // factor in [0,1]
color color_lighten(color c, float lightness_factor); // factor in [0,1]

