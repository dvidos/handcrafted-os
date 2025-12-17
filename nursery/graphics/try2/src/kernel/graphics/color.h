#pragma once
#include <stdint.h>

typedef uint32_t color; // first byte = opacity, FF=opaque, 00=transparect

static inline uint8_t color_a(color c) { return (c >> 24); }
static inline uint8_t color_r(color c) { return (c >> 16); }
static inline uint8_t color_g(color c) { return (c >>  8); }
static inline uint8_t color_b(color c) { return (c >>  0); }
static inline color   color_argb(uint8_t alpha, uint8_t red, uint8_t green, uint8_t blue) { return (((uint32_t)alpha) << 24) | (((uint32_t)red) << 16) | (((uint32_t)green) << 8) | (((uint32_t)blue) << 0); }
static inline color   color_rgb(uint8_t red, uint8_t green, uint8_t blue) { return color_argb(0xFF, red, green, blue); }
static inline color   color_with_alpha(uint8_t alpha, color clr) { return (((uint32_t)alpha) << 24) | (clr & 0xFFFFFF); }
static inline color   color_black() { return 0xFF000000; }
static inline color   color_white() { return 0xFFFFFFFF; }
static inline color   color_transparent() { return 0x00000000; }
static inline uint8_t color_blend_channel(uint8_t bottom, uint8_t top, uint8_t alpha) { return (((uint32_t)bottom * (255 - alpha)) + ((uint32_t)top * (alpha))) / 255; }


// from settings of linux terminal, dark tango theme
static inline color color_tango_black()          { return 0xFF2e3436; }
static inline color color_tango_red()            { return 0xFFcc0000; }
static inline color color_tango_green()          { return 0xFF4e9a06; }
static inline color color_tango_yellow()         { return 0xFFc4a000; }
static inline color color_tango_blue()           { return 0xFF3465a4; }
static inline color color_tango_magenta()        { return 0xFF75507b; }
static inline color color_tango_cyan()           { return 0xFF06989a; }
static inline color color_tango_white()          { return 0xFFd3d7cf; }
static inline color color_tango_dark_gray()      { return 0xFF555753; }
static inline color color_tango_bright_red()     { return 0xFFef2929; }
static inline color color_tango_bright_green()   { return 0xFF8ae234; }
static inline color color_tango_bright_yellow()  { return 0xFFfce94f; }
static inline color color_tango_bright_blue()    { return 0xFF729fcf; }
static inline color color_tango_bright_magenta() { return 0xFFad7fa8; }
static inline color color_tango_bright_cyan()    { return 0xFF34e2e2; }
static inline color color_tango_bright_white()   { return 0xFFeeeeec; }


color color_between(color c1, color c2, float distance_factor); // factor in [0,1]
color color_darken(color c, float darkness_factor); // factor in [0,1]
color color_lighten(color c, float lightness_factor); // factor in [0,1]

