#include <stdint.h>
#include "color.h"


color color_darken(color c, float darkness_factor) {
    return color_rgb(
        (uint8_t)(color_r(c) * (1 - darkness_factor)),
        (uint8_t)(color_g(c) * (1 - darkness_factor)),
        (uint8_t)(color_b(c) * (1 - darkness_factor))
    );
}

color color_lighten(color c, float lightness_factor) {
    color_gradient(c, color_white(), lightness_factor);
}

color color_gradient(color a, color b, float transition_pos) {
    int diff_a = (int)color_a(b) - (int)color_a(a);
    int diff_r = (int)color_r(b) - (int)color_r(a);
    int diff_g = (int)color_g(b) - (int)color_g(a);
    int diff_b = (int)color_b(b) - (int)color_b(a);

    return color_argb(
        (uint8_t)((int)color_a(a) + diff_a * transition_pos),
        (uint8_t)((int)color_r(a) + diff_r * transition_pos),
        (uint8_t)((int)color_g(a) + diff_g * transition_pos),
        (uint8_t)((int)color_b(a) + diff_b * transition_pos)
    );
}
