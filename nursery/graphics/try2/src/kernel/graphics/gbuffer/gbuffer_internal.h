#pragma once

#include "../../fundamentals.h"
#include "../../concepts/logger.h"
#include "../../memory/malloc.h"
#include "../../memory/string.h"
#include "../../devices/serial.h"
#include "../fonts/font8x16.h"
#include "../cursors/mouse_cursor.h"
#include "../icons/icon32.h"

#include "../gbuffer.h"


static inline uint32_t *_pixel_ptr(const gbuffer *gb, int x, int y) { return gb->buffer_argb + (y * gb->area.width) + x; }
static inline uint32_t *_pixel_pt_ptr(const gbuffer *gb, point p) { return gb->buffer_argb + (p.y * gb->area.width) + p.x; }
static inline uint32_t *_replace_pixel(uint32_t *ptr, color clr)   { *ptr++ = clr; return ptr; }
static inline uint32_t *_blend_pixel(uint32_t *ptr, color clr)   { *ptr++ = color_blend(*ptr, clr); return ptr; }
static inline uint32_t *_set_pixel_row(uint32_t *ptr, uint32_t clr, int length)   { while (length-- > 0) { *ptr++ = clr; } return ptr; }
static inline color _get_pixel(uint32_t *ptr) { return (color)*ptr; }
static inline uint32_t *_skip_pixel(uint32_t *ptr) { return ptr + 1; }
static inline void _copy_pixel_row(uint32_t *dest, uint32_t *src, int length) {  while (length-- > 0) { *dest++ = *src++; } }

#define clamp01(val)       ((val) > 1.0f) ? 1.0f : (((val) < 0.0f) ? 0.0f : (val))
#define clamp255(val)      ((val) > 255) ? 255 : (((val) < 0) ? 0 : (val))

// ----------------------------------------------------

static gbuffer *global_aux_buffer = 0;


