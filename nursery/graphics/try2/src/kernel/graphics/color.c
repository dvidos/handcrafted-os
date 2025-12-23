#include <stdint.h>
#include "color.h"
#include "../memory/malloc.h"


typedef struct color_lookups {
    uint8_t applied_alphas[256][256];
} color_lookups;
static color_lookups *lookups;


void initialize_colors() {
    lookups = kmalloc(sizeof(color_lookups));

    // many operations in graphics boil down to a float division: "result = x * (y / 255)",
    // essentially, y defines the resulting percentage (per byte) of x.
    // since this division is expensive, we use precomputed lookup tables.
    // same happens when two alphas are blended together

    for (int alpha = 0; alpha < 256; alpha++) {
        for (int color = 0; color < 256; color++) {
            lookups->applied_alphas[color][alpha] = (color * alpha) / 255;
        }
    }
}
static inline uint8_t _apply_alpha(uint8_t c, uint8_t alpha) {
    return lookups->applied_alphas[c][alpha];
}


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

color color_blend(color bottom, color top) {
    // Porter-Duff "over" algorithm:
    // R.a = B.a + A.a * (1 - B.a)
    // R.rgb = (B.rgb * B.a + A.rgb * A.a * (1 - B.a)) / R.a

    uint32_t top_a = color_a(top);
    uint32_t top_a_inv = 255 - top_a;
    uint32_t top_r = color_r(top);
    uint32_t top_g = color_g(top);
    uint32_t top_b = color_b(top);

    if      (top_a == 0xFF) return top;
    else if (top_a == 0x00) return bottom;

    uint32_t bottom_a = color_a(bottom);
    uint32_t bottom_r = color_r(bottom);
    uint32_t bottom_g = color_g(bottom);
    uint32_t bottom_b = color_b(bottom);

    // top
    
    // result
    uint32_t result_a = top_a + (bottom_a * top_a_inv) / 255;
    uint32_t result_r = 0;
    uint32_t result_g = 0;
    uint32_t result_b = 0;

    if (result_a > 0) {       
        result_r = (top_r * top_a + bottom_r * bottom_a * top_a_inv / 255) / result_a;
        result_g = (top_g * top_a + bottom_g * bottom_a * top_a_inv / 255) / result_a;
        result_b = (top_b * top_a + bottom_b * bottom_a * top_a_inv / 255) / result_a;
    }
    
    return color_argb((uint8_t)result_a, (uint8_t)result_r, (uint8_t)result_g, (uint8_t)result_b);
}
