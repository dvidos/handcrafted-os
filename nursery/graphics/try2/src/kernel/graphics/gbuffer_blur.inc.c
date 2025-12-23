#include <stdint.h>
#include "gbuffer.h"
#include "../serial/serial.h"


typedef struct blur_window {
    uint32_t alpha;
    uint32_t red;
    uint32_t green;
    uint32_t blue;
    int size;
} blur_window;


static inline void blur_window_clear(blur_window *w) {
    w->alpha = 0;
    w->red   = 0;
    w->green = 0;
    w->blue  = 0;
    w->size  = 0;
}

static inline void blur_window_add(blur_window *w, uint32_t *pixel) {
    w->alpha += color_a(*pixel);
    w->red   += color_r(*pixel);
    w->green += color_g(*pixel);
    w->blue  += color_b(*pixel);
    w->size  += 1;
}

static inline void blur_window_remove(blur_window *w, uint32_t *pixel) {
    w->alpha -= color_a(*pixel);
    w->red   -= color_r(*pixel);
    w->green -= color_g(*pixel);
    w->blue  -= color_b(*pixel);
    w->size  -= 1;
}

static inline void blur_window_add_and_remove(blur_window *w, uint32_t *add, uint32_t *remove) {
    w->alpha += color_a(*add) - color_a(*remove);
    w->red   += color_r(*add) - color_r(*remove);
    w->green += color_g(*add) - color_g(*remove);
    w->blue  += color_b(*add) - color_b(*remove);
}

typedef void blur_window_apply_func(blur_window *w, uint32_t *pixel);

static void blur_window_apply_color(blur_window *w, uint32_t *pixel) {
    if (w->size == 0) { _replace_pixel(pixel, 0xFFFF0000); return; }
    _replace_pixel(pixel, color_argb(color_a(_get_pixel(pixel)), w->red / w->size, w->green / w->size, w->blue / w->size));
}

static void blur_window_apply_alpha(blur_window *w, uint32_t *pixel) {
    if (w->size == 0) { _replace_pixel(pixel, 0xFFFF0000); return; }
    _replace_pixel(pixel, color_with_alpha(w->alpha / w->size, _get_pixel(pixel)));
}

static inline void __debug_blur(gbuffer *src, gbuffer *dest, int x, int y, int x_start, int x_end, int radius, blur_window *win, char *extra) {
    serial_print_str("blur: rng="); serial_print_int(x_start);
    serial_print_str(".."); serial_print_int(x_end);
    serial_print_str("/rds="); serial_print_int(radius);
    serial_print_str(", x:y="); serial_print_int(x);
    serial_print_str(":"); serial_print_int(y);
    serial_print_str(", src_px="); serial_print_hex32(_get_pixel(_pixel_ptr(src, x, y)));
    serial_print_str(", wsize="); serial_print_int(win->size);
    if (win->size > 0) {
        serial_print_str(", wavg="); 
        serial_print_hex32(color_argb(
            win->alpha / win->size,
            win->red   / win->size,
            win->green / win->size,
            win->blue  / win->size
        ));
    }
    serial_print_str(", dest_px="); serial_print_hex32(_get_pixel(_pixel_ptr(dest, x, y)));
    if (extra) {
        serial_print_str(", ");
        serial_print_str(extra);
    }
    serial_print_str("\r\n");
}

typedef uint32_t *blur_get_axis_pixel_func(gbuffer *gb, int primary_axis, int secondary_axis);

static uint32_t *blur_get_axis_pixel_hor(gbuffer *gb, int primary_axis, int secondary_axis) {
    return _pixel_ptr(gb, primary_axis, secondary_axis); // primary is x
}
static uint32_t *blur_get_axis_pixel_ver(gbuffer *gb, int primary_axis, int secondary_axis) {
    return _pixel_ptr(gb, secondary_axis, primary_axis); // primary is y
}



static inline void blur_window_box_algorithm_hor(gbuffer *src, gbuffer *dest, garea rect, int radius, blur_window_apply_func apply_blur) {
    // this method mirrors the same for vertical
    blur_window win;
    
    for (int y = rect.origin.y; y < rect.origin.y + rect.size.height; y++) {
        const int x_start = rect.origin.x;
        const int x_end   = rect.origin.x + rect.size.width;
        int x;
        int x_win_left;
        int x_win_right;
        
        blur_window_clear(&win);
        __debug_blur(src, dest, x, y, x_start, x_end, radius, &win, "init");

        // Prime window with pixels prior to the first pixel (partial window)
        for (int i = radius; i > 0; i--) {
            int xi = x_start - i;
            if (xi < 0)
                continue;

            blur_window_add(&win, _pixel_ptr(src, xi, y));
            __debug_blur(src, dest, x, y, x_start, x_end, radius, &win, "priming");
        }

        // Phase 1: growing window (left edge),  x = [x_start .. x_start+radius-1]
        for (x = x_start; x <= x_start + radius && x < x_end; x++) {
            x_win_right = x + radius;
            if (x_win_right < x_end)
                blur_window_add(&win, _pixel_ptr(src, x_win_right, y));
            apply_blur(&win, _pixel_ptr(dest, x, y));
            __debug_blur(src, dest, x, y, x_start, x_end, radius, &win, "growing");
        }

        // Phase 2: full window, fast path, x = [x_start+radius .. x_end-radius-1]
        for (; x + radius < x_end; x++) {
            x_win_left = x - radius - 1;  // add right, remove left
            x_win_right = x + radius;

            blur_window_add_and_remove(&win, _pixel_ptr(src, x_win_right, y), _pixel_ptr(src, x_win_left, y));
            apply_blur(&win, _pixel_ptr(dest, x, y));
            __debug_blur(src, dest, x, y, x_start, x_end, radius, &win, "stepping");
        }

        // Phase 3: shrinking window (right edge), x = [x_end-radius .. x_end-1]
        for (; x < x_end; x++) {
            x_win_left = x - radius - 1;
            x_win_right = x + radius;

            if (x_win_left >= 0)
                blur_window_remove(&win, _pixel_ptr(src, x_win_left, y));
            if (x_win_right < src->area.size.width)
                blur_window_add(&win, _pixel_ptr(src, x_win_right, y));
            apply_blur(&win, _pixel_ptr(dest, x, y));
            __debug_blur(src, dest, x, y, x_start, x_end, radius, &win, "shrinking");
        }
    }
}



static inline void blur_window_box_algorithm_ver(gbuffer *src, gbuffer *dest, garea rect, int radius, blur_window_apply_func apply_blur) {
    // this method mirrors the same for vertical
    blur_window win;

    for (int x = rect.origin.x; x < rect.origin.x + rect.size.width; x++) {
        blur_window_clear(&win);

        const int y_start = rect.origin.y;
        const int y_end   = rect.origin.y + rect.size.height;
        int y;
        int y_win_first;
        int y_win_last;

        // Prime window with pixels prior to the first pixel (partial window)
        for (int i = radius; i > 0; i--) {
            int yi = y_start - i;
            if (yi >= 0)
                blur_window_add(&win, _pixel_ptr(src, x, yi));
        }

        // Phase 1: growing window (left edge),  x = [x_start .. x_start+radius-1]
        for (y = y_start; y < y_start + radius && y < y_end; y++) {
            y_win_last = y + radius;
            if (y_win_last < y_end)
                blur_window_add(&win, _pixel_ptr(src, x, y_win_last));
            apply_blur(&win, _pixel_ptr(dest, x, y));
        }

        // Phase 2: full window, fast path, x = [x_start+radius .. x_end-radius-1]
        for (; y + radius < y_end; y++) {
            y_win_first = y - radius - 1;  // add right, remove left
            y_win_last = y + radius;
            blur_window_add_and_remove(&win, _pixel_ptr(src, x, y_win_last), _pixel_ptr(src, x, y_win_first));
            apply_blur(&win, _pixel_ptr(dest, x, y));
        }

        // Phase 3: shrinking window (right edge), x = [x_end-radius .. x_end-1]
        for (; y < y_end; y++) {
            y_win_first = y - radius - 1;
            if (y_win_first >= 0)
                blur_window_remove(&win, _pixel_ptr(src, x, y_win_first));
            if (y_win_last < src->area.size.height)
                blur_window_add(&win, _pixel_ptr(src, x, y_win_last));
            apply_blur(&win, _pixel_ptr(dest, x, y));
        }
    }
}

