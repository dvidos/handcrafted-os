#include <stdint.h>
#include "color.h"

color color_rgba(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha) {
    return ((alpha & 0xFF) << 24) | ((red & 0xFF) << 16) | ((green & 0xFF) << 8) | (blue & 0xFF);
}

color color_rgb(uint8_t red, uint8_t green, uint8_t blue) {
    return 0xFF000000 | ((red & 0xFF) << 16) | ((green & 0xFF) << 8) | (blue & 0xFF);
}

color color_black()       { return 0xFF000000; }
color color_gray()        { return 0xFF888888; }
color color_white()       { return 0xFFFFFFFF; }
color color_transparent() { return 0x00000000; }

color color_between(color c1, color c2, float distance_factor) {
    uint8_t r1 = (c1 >> 16) & 0xFF;
    uint8_t g1 = (c1 >>  8) & 0xFF;
    uint8_t b1 = (c1 >>  0) & 0xFF;

    uint8_t r2 = (c2 >> 16) & 0xFF;
    uint8_t g2 = (c2 >>  8) & 0xFF;
    uint8_t b2 = (c2 >>  0) & 0xFF;
    
    uint8_t red   = r1 + distance_factor * (r2 - r1);
    uint8_t green = g1 + distance_factor * (g2 - g1);
    uint8_t blue  = b1 + distance_factor * (b2 - b1);

    return 0xFF000000 | (red << 16) | (green << 8) | blue;
}

color color_darken(color c, float darkness_factor) {
    return RGBA(
        (uint8_t)(RGB_R(c) * (1 - darkness_factor)),
        (uint8_t)(RGB_G(c) * (1 - darkness_factor)),
        (uint8_t)(RGB_B(c) * (1 - darkness_factor)),
        RGB_A(c)
    );
}

color color_lighten(color c, float lightness_factor) {
    color_between(c, color_white(), lightness_factor);
}
