#pragma once
#include <stdint.h>

typedef uint32_t color; // first byte = opacity, FF=opaque, 00=transparect

static inline uint8_t color_a(color c) { return (c >> 24); }
static inline uint8_t color_r(color c) { return (c >> 16); }
static inline uint8_t color_g(color c) { return (c >>  8); }
static inline uint8_t color_b(color c) { return (c >>  0); }
static inline color   color_argb(uint8_t alpha, uint8_t red, uint8_t green, uint8_t blue) { return (((uint32_t)alpha) << 24) | (((uint32_t)red) << 16) | (((uint32_t)green) << 8) | (((uint32_t)blue) << 0); }
static inline color   color_rgb(uint8_t red, uint8_t green, uint8_t blue) { return color_argb(0xFF, red, green, blue); }
static inline color   color_black() { return 0xFF000000; }
static inline color   color_white() { return 0xFFFFFFFF; }
static inline color   color_transparent() { return 0x00000000; }



// TODO: bring online the 16 colors from the Tango palette that we have in our Screenshots (e.g. inline color_tango_red())
// TODO: also do this pattern for inlines in headers:


color color_between(color c1, color c2, float distance_factor); // factor in [0,1]
color color_darken(color c, float darkness_factor); // factor in [0,1]
color color_lighten(color c, float lightness_factor); // factor in [0,1]

