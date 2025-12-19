#pragma once

// origin (top left) is inclusive everywhere
// bottom right is exclusive everywhere (e.g. similar to 256 is exclusive and 0xFF is inclusive)

typedef struct gpoint {
    int x;
    int y;
} gpoint;

typedef struct gsize {
    int width;
    int height;
} gsize;

typedef struct garea {
    gpoint origin;
    gsize size;
} garea;


static inline gpoint gpoint_of(int x, int y)                { return (gpoint){.x = x, .y = y}; }
static inline gpoint gpoint_zero()                          { return (gpoint){.x = 0, .y = 0}; }
static inline gpoint gpoint_move(gpoint p, int dx, int dy)  { return (gpoint){.x = p.x + dx, .y = p.y + dy}; }
static inline int    gpoint_is_inside(gpoint p, garea a)    { return p.x >= a.origin.x && p.x < a.origin.x + a.size.width && p.y >= a.origin.y && p.y < a.origin.y + a.size.height; }
static inline gpoint gpoint_to_local(gpoint p, garea container)  { return gpoint_of(p.x - container.origin.x, p.y - container.origin.y); }
static inline gpoint gpoint_to_global(gpoint p, garea container) { return gpoint_of(p.x + container.origin.x, p.y + container.origin.y); }


static inline gsize gsize_of(int w, int h)  { return (gsize){.width = w, .height = h}; }
static inline gsize gsize_zero()            { return (gsize){.width = 0, .height = 0}; }
static inline int   gsize_is_empty(gsize s) { return s.width <= 0 || s.height <= 0; }
static inline gsize gsize_grow(gsize s, int dx, int dy) { return (gsize){.width = s.width + dx, .height = s.height + dy}; }
static inline gsize gsize_grow_by(gsize s, int delta)   { return (gsize){.width = s.width + delta, .height = s.height + delta}; }
static inline gsize gsize_normalize(gsize s)            { return (gsize){.width = s.width < 0 ? 0 : s.width, .height = s.height < 0 ? 0 : s.height}; }


static inline garea garea_of(int x, int y, int w, int h) { return (garea){.origin = gpoint_of(x, y), .size = gsize_of(w, h)}; }
static inline int   garea_is_empty(garea a)              { return gsize_is_empty(a.size); }
static inline garea garea_grow(garea a, int dx, int dy)  { return (garea){.origin = gpoint_move(a.origin, -dx, -dy), .size = gsize_grow(a.size, 2*dx, 2*dy)}; }
static inline garea garea_move(garea a, int dx, int dy)  { return (garea){.origin = gpoint_move(a.origin, dx, dy), .size = a.size}; }
static inline garea garea_normalize(garea a)             { return (garea){.origin = a.origin, .size = gsize_normalize(a.size)}; }
static inline garea garea_to_local(garea a, garea container)  { return (garea){.origin = gpoint_to_local(a.origin, container), .size = a.size}; }
static inline garea garea_to_global(garea a, garea container) { return (garea){.origin = gpoint_to_global(a.origin, container), .size = a.size}; }
static inline gpoint garea_top_right(garea a)    { return gpoint_of(a.origin.x + a.size.width - 1, a.origin.y); }
static inline gpoint garea_bottom_left(garea a)  { return gpoint_of(a.origin.x, a.origin.y + a.size.height - 1); }
static inline gpoint garea_bottom_right(garea a) { return gpoint_of(a.origin.x + a.size.width - 1, a.origin.y + a.size.height - 1); }
static inline gpoint garea_bottom_right_exclusive(garea a) { return gpoint_of(a.origin.x + a.size.width, a.origin.y + a.size.height); }
// static inline garea garea_intersect(garea a, garea b);
// static inline garea garea_union(garea a, garea b);
// splits backround into zero to four areas, for redrawing, returns number of resulting areas 
// static inline int garea_subtract(garea bg, garea fg, garea out[4]);

static inline int garea_is_completely_outside(garea a, garea container) { 
    return (
        a.origin.x >= container.origin.x + container.size.width ||
        a.origin.y >= container.origin.y + container.size.height ||
        a.origin.x + a.size.width  < container.origin.x ||
        a.origin.y + a.size.height < container.origin.y 
    );
}

static inline int garea_is_completely_inside(garea a, garea container) { 
    return (
        a.origin.x >= container.origin.x &&
        a.origin.y >= container.origin.y &&
        a.origin.x + a.size.width  <= container.origin.x + container.size.width &&
        a.origin.y + a.size.height <= container.origin.y + container.size.height 
    );
}

static inline garea garea_crop(garea a, garea viewport) {
    if (a.origin.x < viewport.origin.x) {
        a.size.width -= (viewport.origin.x - a.origin.x);
        a.origin.x = viewport.origin.x;
    }
    if (a.origin.y < viewport.origin.y) {
        a.size.height -= (viewport.origin.y - a.origin.y);
        a.origin.y = viewport.origin.y;
    }
    if (a.origin.x + a.size.width > viewport.origin.x + viewport.size.width) {
        a.size.width -= (a.origin.x + a.size.width - (viewport.origin.x + viewport.size.width));
    }
    if (a.origin.y + a.size.height > viewport.origin.y + viewport.size.height) {
        a.size.height -= (a.origin.y + a.size.height - (viewport.origin.y + viewport.size.height));
    }
    return garea_normalize(a);
}


// we can also make an iterator for an area inside a buffer, we'll see.
/*
typedef struct pixel_iter {
    gbuffer *gb;
    garea area;
    int x, y;
} pixel_iter;

static inline void pixel_iter_init(pixel_iter *it, gbuffer *gb, garea a) {
    it->gb = gb;
    it->area = garea_intersect(a, garea_of(0,0,gb->width,gb->height)); // crop
    it->x = it->area.origin.x;
    it->y = it->area.origin.y;
}

static inline uint32_t *pixel_iter_next(pixel_iter *it) {
    if (it->y >= it->area.origin.y + it->area.size.height)
        return NULL; // done
    uint32_t *p = _pixel_ptr(it->gb, it->x, it->y);
    it->x++;
    if (it->x >= it->area.origin.x + it->area.size.width) {
        it->x = it->area.origin.x;
        it->y++;
    }
    return p;
}

pixel_iter it;
pixel_iter_init(&it, gb, some_area);
uint32_t *px;
while ((px = pixel_iter_next(&it)) != NULL) {
    *px = some_color;
}

#define FOR_PIXELS_IN_AREA(gb, area, px) \
    for (pixel_iter _it, *px = (pixel_iter_init(&_it, gb, area), pixel_iter_next(&_it)); \
         px; \
         px = pixel_iter_next(&_it))

*/