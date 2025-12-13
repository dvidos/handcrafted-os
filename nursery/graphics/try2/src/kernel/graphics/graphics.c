#include <stdint.h>
#include "graphics.h"
#include "color.h"
#include "../memory/string.h"

struct graphics_surface {
    int width;
    int height;
    uint32_t *pixels_array;  // each pixel colors RGBA color
    int opacity;
    int blur_radius;
    int shadow_radius;
};

gs *new_graphics_surface(int width, int height);

int gs_text(gs *g, int x, int y, const char *text, color color);
int gs_fill(gs *g, color color) {
    int len_uint32s = g->width * g->height;
    memset(g->pixels_array, color, len_uint32s * sizeof(uint32_t));
    return 0;
}

int gs_rect(gs *g, int x, int y, int width, int height, int corner_radius, color color);
int gs_frame(gs *g, int x, int y, int width, int height, int corner_radius, color color);
int gs_gradient_rect(gs *g, int x, int y, int width, int height, color center, float variance_factor, int direction);
int gs_copy_gs(gs *target, gs *sprite, int x, int y, int transparency, int blurryness, int shadow_distance, int shadow_radius);
