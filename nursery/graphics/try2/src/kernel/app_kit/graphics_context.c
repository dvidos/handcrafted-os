#include "../memory/malloc.h"
#include "graphics_context.h"


graphics_context_t *new_graphics_context(gbuffer *gb) {
    graphics_context_t *gc = kmalloc(sizeof(graphics_context_t));
    gc->buffer = gb;
    return gc;
}
