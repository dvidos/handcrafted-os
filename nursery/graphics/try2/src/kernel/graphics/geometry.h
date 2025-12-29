#pragma once

// origin (top left) is inclusive everywhere, bottom right is exclusive everywhere 
// e.g. similar to 256 is exclusive and 0xFF is inclusive


static inline int min(int a, int b) { return a < b ? a : b; }
static inline int max(int a, int b) { return a > b ? a : b; }

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
static inline point point_translate(point p, point delta)    { return (point){.x = p.x + delta.x, .y = p.y + delta.y}; }
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
static inline area  area_zero()                             { return (area){.x = 0, .y = 0, .width = 0, .height = 0}; }
static inline area  area_with(point p, size s)              { return (area){.x = p.x, .y = p.y, .width = s.width, .height = s.height}; }
static inline int   area_is_empty(area a)                   { return a.width <= 0 || a.height <= 0; }
static inline int   area_contains(area a, point p)          { return p.x >= a.x && p.x < a.x + a.width && p.y >= a.y && p.y < a.y + a.height; }
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
