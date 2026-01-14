#pragma once
#include "../fundamentals.h"

// origin (top left) is inclusive everywhere, bottom right is exclusive everywhere 
// e.g. similar to 256 is exclusive and 0xFF is inclusive


// static inline int min(int a, int b) { return a < b ? a : b; }
// static inline int max(int a, int b) { return a > b ? a : b; }

typedef struct point {
    int x;
    int y;
} point;

typedef struct vector {
    int dx;
    int dy;
} vector;

typedef struct vectorf {
    float dx;
    float dy;
} vectorf;

typedef struct size {
    int width;
    int height;
} size;

typedef struct area {
    int x;
    int y;
    int width;
    int height;
} area;

typedef float factor; // in range [0..1], inclusively

typedef enum alignment {
    ALIGN_TOP_LEFT,
    ALIGN_MIDDLE_LEFT,
    ALIGN_BOTTOM_LEFT,
    ALIGN_TOP_CENTER,
    ALIGN_MIDDLE_CENTER,
    ALIGN_BOTTOM_CENTER,
    ALIGN_TOP_RIGHT,
    ALIGN_MIDDLE_RIGHT,
    ALIGN_BOTTOM_RIGHT,
} alignment;

static inline point point_of(int x, int y)                   { return (point){.x = x, .y = y}; }
static inline point point_zero()                             { return (point){.x = 0, .y = 0}; }
static inline point point_move(point p, int dx, int dy)      { return (point){.x = p.x + dx, .y = p.y + dy}; }
static inline point point_translate(point p, point delta)    { return (point){.x = p.x + delta.x, .y = p.y + delta.y}; }
static inline bool  point_is_inside(point p, area a)         { return p.x >= a.x && p.x < a.x + a.width && p.y >= a.y && p.y < a.y + a.height; }
static inline point point_to_local(point p, area container)  { return point_of(p.x - container.x, p.y - container.y); }
static inline point point_to_global(point p, area container) { return point_of(p.x + container.x, p.y + container.y); }

static inline vector vector_of(int dx, int dy)               { return (vector){.dx = dx, .dy = dy}; }
static inline vector vector_zero()                           { return (vector){.dx = 0, .dy = 0}; }
static inline bool   vector_is_zero(vector v)                { return (v.dx == 0 && v.dy == 0); }
static inline vector vector_from_to(point a, point b)        { return (vector){.dx = b.x - a.x, .dy = b.y - a.y}; }
static inline int    vector_squared_distance(vector v)       { return (v.dx * v.dx) + (v.dy * v.dy); }

static inline vectorf vectorf_of(float dx, float dy)            { return (vectorf){.dx = dx, .dy = dy}; }
static inline vectorf vectorf_zero()                            { return (vectorf){.dx = 0, .dy = 0}; }
static inline bool    vectorf_is_zero(vectorf v)                { return (v.dx == 0 && v.dy == 0); }
static inline vectorf vectorf_from_to(point a, point b)         { return (vectorf){.dx = b.x - a.x, .dy = b.y - a.y}; }
static inline float   vectorf_dot_product(vectorf a, vectorf b) { return a.dx * b.dx + a.dy * b.dy; }

static inline size size_of(int w, int h)                    { return (size){.width = w, .height = h}; }
static inline size size_zero()                              { return (size){.width = 0, .height = 0}; }
static inline bool size_is_empty(size s)                    { return s.width <= 0 || s.height <= 0; }
static inline size size_grow(size s, int dx, int dy)        { return (size){.width = s.width + dx, .height = s.height + dy}; }
static inline size size_grow_by(size s, int delta)          { return (size){.width = s.width + delta, .height = s.height + delta}; }

static inline area  area_of(int x, int y, int w, int h)     { return (area){.x = x, .y = y, .width = w, .height = h}; }
static inline area  area_zero()                             { return (area){.x = 0, .y = 0, .width = 0, .height = 0}; }
static inline area  area_with(point p, size s)              { return (area){.x = p.x, .y = p.y, .width = s.width, .height = s.height}; }
static inline bool  area_is_empty(area a)                   { return a.width <= 0 || a.height <= 0; }
static inline bool  area_contains(area a, point p)          { return p.x >= a.x && p.x < a.x + a.width && p.y >= a.y && p.y < a.y + a.height; }
static inline area  area_between(point p1, point p2)        { return (area){.x = min(p1.x,p2.x), .y = min(p1.y,p2.y), .width = max(p1.x,p2.x) - min(p1.x,p2.x), .height = max(p1.y,p2.y) - min(p1.y,p2.y)}; }
static inline area  area_grow(area a, int dx, int dy)       { return (area){.x = a.x - dx, .y = a.y - dy, .width = a.width + 2*dx, .height = a.height + 2*dy}; }
static inline area  area_move(area a, int dx, int dy)       { return (area){.x = a.x + dx, .y = a.y + dy, .width = a.width, .height = a.height}; }
static inline area  area_translate(area a, point delta)     { return (area){.x = a.x + delta.x, .y = a.y + delta.y, .width = a.width, .height = a.height}; }
static inline area  area_to_local(area a, area container)   { return (area){.x = a.x - container.x, .y = a.y - container.y, .width = a.width, .height = a.height}; }
static inline area  area_to_global(area a, area container)  { return (area){.x = a.x + container.x, .y = a.y + container.y, .width = a.width, .height = a.height}; }
static inline point area_location(area a)                   { return (point){.x = a.x, .y = a.y}; }
static inline size  area_size(area a)                       { return (size){.width = a.width, .height = a.height}; }
static inline point area_top_right(area a)                  { return point_of(a.x + a.width - 1, a.y); }
static inline point area_bottom_left(area a)                { return point_of(a.x, a.y + a.height - 1); }
static inline point area_bottom_right(area a)               { return point_of(a.x + a.width - 1, a.y + a.height - 1); }
static inline point area_bottom_right_exclusive(area a)     { return point_of(a.x + a.width, a.y + a.height); }


static inline void area_split_rounded_fill_areas(area rect, int radius, area *top, area *middle, area *bottom, area *top_left, area *top_right, area *bottom_left, area *bottom_right) {
    *top          = area_of(rect.x + radius, rect.y,  rect.width - (radius * 2), radius ); 
    *middle       = area_of(rect.x, rect.y + radius, rect.width, rect.height - (radius * 2)); // includes left and right, for performance
    *bottom       = area_of(rect.x + radius, rect.y + rect.height - radius, rect.width - (radius * 2), radius );

    *top_left     = area_of(rect.x,                       rect.y,                        radius, radius);
    *top_right    = area_of(rect.x + rect.width - radius, rect.y,                        radius, radius);
    *bottom_left  = area_of(rect.x,                       rect.y + rect.height - radius, radius, radius);
    *bottom_right = area_of(rect.x + rect.width - radius, rect.y + rect.height - radius, radius, radius);
}
static inline void area_split_rounded_border_areas(area rect, int radius, int thickness, area *top, area *bottom, area *left, area *right, area *top_left, area *top_right, area *bottom_left, area *bottom_right) {
    *top          = area_of(rect.x + radius, rect.y, rect.width - (radius * 2), thickness); 
    *bottom       = area_of(rect.x + radius, rect.y + rect.height - thickness, rect.width - (radius * 2), thickness);
    *left         = area_of(rect.x, rect.y + radius, thickness, rect.height - (radius * 2));
    *right        = area_of(rect.x + rect.width - thickness, rect.y + radius, thickness, rect.height - (radius * 2));

    *top_left     = area_of(rect.x,                       rect.y,                        radius, radius);
    *top_right    = area_of(rect.x + rect.width - radius, rect.y,                        radius, radius);
    *bottom_left  = area_of(rect.x,                       rect.y + rect.height - radius, radius, radius);
    *bottom_right = area_of(rect.x + rect.width - radius, rect.y + rect.height - radius, radius, radius);
}



// static inline area area_intersect(area a, area b);
// static inline area area_union(area a, area b);
// splits backround into zero to four areas, for redrawing, returns number of resulting areas 
// static inline int area_subtract(area bg, area fg, area out[4]);

static inline area area_intersect(area a, area b) {
    int x1 = max(a.x, b.x);
    int y1 = max(a.y, b.y);
    int x2 = min(a.x + a.width,  b.x + b.width);
    int y2 = min(a.y + a.height, b.y + b.height);

    // returns empty area if there is no intersection
    return (x2 <= x1 || y2 <= y1) ? area_zero() : area_of(x1, y1, x2 - x1, y2 - y1);
}

static inline area area_union(area a, area b) {
    if (a.width == 0 || a.height == 0) return b;
    if (b.width == 0 || b.height == 0) return a;
    int x1 = min(a.x, b.x);
    int y1 = min(a.y, b.y);
    int x2 = max(a.x + a.width,  b.x + b.width);
    int y2 = max(a.y + a.height, b.y + b.height);

    // the smallest rectangle that contains both areas
    return (area){ x1, y1, x2 - x1, y2 - y1 };
}


static inline int area_is_completely_outside(area a, area container) { 
    return (
        a.x >= container.x + container.width ||
        a.y >= container.y + container.height ||
        a.x + a.width  < container.x ||
        a.y + a.height < container.y 
    );
}

static inline int area_is_completely_inside(area a, area container) { 
    return (
        a.x >= container.x &&
        a.y >= container.y &&
        a.x + a.width  <= container.x + container.width &&
        a.y + a.height <= container.y + container.height 
    );
}

static inline area area_crop(area a, area viewport) {
    if (a.x < viewport.x) {
        a.width -= (viewport.x - a.x);
        a.x = viewport.x;
    }
    if (a.y < viewport.y) {
        a.height -= (viewport.y - a.y);
        a.y = viewport.y;
    }
    if (a.x + a.width > viewport.x + viewport.width) {
        a.width -= (a.x + a.width - (viewport.x + viewport.width));
    }
    if (a.y + a.height > viewport.y + viewport.height) {
        a.height -= (a.y + a.height - (viewport.y + viewport.height));
    }

    if (a.width  < 0) a.width  = 0;
    if (a.height < 0) a.height = 0;

    return a;
}

static area area_align(area container, size floaty, alignment align) {
    int x = 0;
    int y = 0;

    // horizontal
    if (align == ALIGN_TOP_LEFT || align == ALIGN_MIDDLE_LEFT || align == ALIGN_BOTTOM_LEFT)
        x = container.x;
    else if (align == ALIGN_TOP_CENTER || align == ALIGN_MIDDLE_CENTER || align == ALIGN_BOTTOM_CENTER)
        x = container.x + (container.width - floaty.width) / 2;
    else if (align == ALIGN_TOP_RIGHT || align == ALIGN_MIDDLE_RIGHT || align == ALIGN_BOTTOM_RIGHT)
        x = container.x + container.width - floaty.width;

    // vertical
    if (align == ALIGN_TOP_LEFT || align == ALIGN_TOP_CENTER || align == ALIGN_TOP_RIGHT)
        y = container.y;
    else if (align == ALIGN_MIDDLE_LEFT || align == ALIGN_MIDDLE_CENTER || align == ALIGN_MIDDLE_RIGHT)
        y = container.y + (container.height - floaty.height) / 2;
    else if (align == ALIGN_BOTTOM_LEFT || align == ALIGN_BOTTOM_CENTER || align == ALIGN_BOTTOM_RIGHT)
        y = container.y + container.height - floaty.height;

    return area_of(x, y, floaty.width, floaty.height);
}

// ------------------------------------------------------------------------------------------

static inline factor   factor_clamp(factor n)           { return (n < 0.0f) ? 0.0f : (n > 1.0f ? 1.0f : n); }
static inline factor   factor_from_u8(uint8_t v)        { return (factor)v / 255.0f; }
static inline factor   factor_from_u16(uint16_t v)      { return (factor)v / 65535.0f; }
static inline factor   factor_from_u32(uint32_t v)      { return (factor)v / 4294967295.0f; }
static inline factor   factor_from_percent(int percent) { return factor_clamp(percent / 100.0f); }
static inline uint8_t  factor_to_u8(factor n)           { return (uint8_t)(factor_clamp(n) * 255.0f + 0.5f); }
static inline uint16_t factor_to_u16(factor n)          { return (uint16_t)(factor_clamp(n) * 65535.0f + 0.5f); }
static inline uint32_t factor_to_u32(factor n)          { return (uint32_t)(factor_clamp(n) * 4294967295.0f + 0.5f); }
static inline factor   factor_invert(factor n)          { return 1.0f - factor_clamp(n); }
static inline factor   factor_mul(factor a, factor b)   { return factor_clamp(a * b); }
static inline int      factor_scale_into_range(factor n, int min, int max)       { return min + (int)(factor_clamp(n) * (max - min) + 0.5f); }
static inline float    factor_scale_into_rangef(factor n, float min, float max)  { return min + factor_clamp(n) * (max - min); }
static inline float    factor_interpolate(float a, float b, factor t)            { return a + factor_clamp(t) * (b - a); }

// ------------------------------------------------------------------------------------------
