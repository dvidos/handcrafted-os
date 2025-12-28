#pragma once

// origin (top left) is inclusive everywhere, bottom right is exclusive everywhere 
// e.g. similar to 256 is exclusive and 0xFF is inclusive

typedef struct point {
    int x;
    int y;
} point;

typedef struct vector {
    float dx;
    float dy;
} vector;

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

static inline point point_of(int x, int y)                   { return (point){.x = x, .y = y}; }
static inline point point_zero()                             { return (point){.x = 0, .y = 0}; }
static inline point point_move(point p, int dx, int dy)      { return (point){.x = p.x + dx, .y = p.y + dy}; }
static inline int   point_is_inside(point p, area a)         { return p.x >= a.x && p.x < a.x + a.width && p.y >= a.y && p.y < a.y + a.height; }
static inline point point_to_local(point p, area container)  { return point_of(p.x - container.x, p.y - container.y); }
static inline point point_to_global(point p, area container) { return point_of(p.x + container.x, p.y + container.y); }

static inline vector vector_from_to(point a, point b)       { return (vector){.dx = b.x - a.x, .dy = b.y - a.y}; }
static inline float  vector_dot_product(vector a, vector b) { return a.dx * b.dx + a.dy * b.dy; }

static inline size size_of(int w, int h)                    { return (size){.width = w, .height = h}; }
static inline size size_zero()                              { return (size){.width = 0, .height = 0}; }
static inline int  size_is_empty(size s)                    { return s.width <= 0 || s.height <= 0; }
static inline size size_grow(size s, int dx, int dy)        { return (size){.width = s.width + dx, .height = s.height + dy}; }
static inline size size_grow_by(size s, int delta)          { return (size){.width = s.width + delta, .height = s.height + delta}; }

static inline area  area_of(int x, int y, int w, int h)     { return (area){.x = x, .y = y, .width = w, .height = h}; }
static inline area  area_with(point p, size s)              { return (area){.x = p.x, .y = p.y, .width = s.width, .height = s.height}; }
static inline int   area_is_empty(area a)                   { return a.width <= 0 || a.height <= 0; }
static inline int   area_contains(area a, point p)          { return p.x >= a.x && p.x < a.x + a.width && p.y >= a.y && p.y < a.y + a.height; }
static inline area  area_grow(area a, int dx, int dy)       { return (area){.x = a.x - dx, .y = a.y - dy, .width = a.width + 2*dx, .height = a.height + 2*dy}; }
static inline area  area_move(area a, int dx, int dy)       { return (area){.x = a.x + dx, .y = a.y + dy, .width = a.width, .height = a.height}; }
static inline area  area_to_local(area a, area container)   { return (area){.x = a.x - container.x, .y = a.y - container.y, .width = a.width, .height = a.height}; }
static inline area  area_to_global(area a, area container)  { return (area){.x = a.x + container.x, .y = a.y + container.y, .width = a.width, .height = a.height}; }
static inline point area_location(area a)                   { return (point){.x = a.x, .y = a.y}; }
static inline size  area_size(area a)                       { return (size){.width = a.width, .height = a.height}; }
static inline point area_top_right(area a)                  { return point_of(a.x + a.width - 1, a.y); }
static inline point area_bottom_left(area a)                { return point_of(a.x, a.y + a.height - 1); }
static inline point area_bottom_right(area a)               { return point_of(a.x + a.width - 1, a.y + a.height - 1); }
static inline point area_bottom_right_exclusive(area a)     { return point_of(a.x + a.width, a.y + a.height); }

// static inline area area_intersect(area a, area b);
// static inline area area_union(area a, area b);
// splits backround into zero to four areas, for redrawing, returns number of resulting areas 
// static inline int area_subtract(area bg, area fg, area out[4]);

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
