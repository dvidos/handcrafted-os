#include <stdint.h>
#include "gbuffer.h"


typedef struct blur_window {
    uint32_t alpha;
    uint32_t red;
    uint32_t green;
    uint32_t blue;
    int size;
} blur_window;

typedef void blur_window_apply_func(blur_window *w, uint32_t *pixel);

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

static void blur_window_apply_color(blur_window *w, uint32_t *pixel) {
    if (w->size == 0) return;
    _replace_pixel(pixel, color_argb(color_a(*pixel), w->red, w->green, w->blue));
}

static void blur_window_apply_alpha(blur_window *w, uint32_t *pixel) {
    if (w->size == 0) return;
    _replace_pixel(pixel, color_with_alpha(w->alpha, *pixel));
}

static inline void blur_window_box_algorithm_hor(gbuffer *src, gbuffer *dest, garea rect, int radius, blur_window_apply_func apply_blur) {
    // this method mirrors the same for vertical
    blur_window win;
    uint32_t *dest_pix;

    for (int y = rect.origin.y; y < rect.origin.y + rect.size.height; y++) {
        blur_window_clear(&win);

        const int x_start = rect.origin.x;
        const int x_end   = rect.origin.x + rect.size.width;
        int x;
        int x_win_first;
        int x_win_last;

        // Prime window with pixels prior to the first pixel (partial window)
        for (int i = radius; i > 0; i--) {
            int xi = x_start - i;
            if (xi >= x_start) {
                blur_window_add(&win, _pixel_ptr(src, xi, y));
            }
        }

        // Phase 1: growing window (left edge),  x = [x_start .. x_start+radius-1]
        for (x = x_start; x < x_start + radius && x < x_end; x++) {
            x_win_last = x + radius;
            if (x_win_last < x_end) {
                blur_window_add(&win, _pixel_ptr(src, x_win_last, y));
            }
            apply_blur(&win, _pixel_ptr(dest, x, y));
        }

        // Phase 2: full window, fast path, x = [x_start+radius .. x_end-radius-1]
        for (; x + radius < x_end; x++) {
            x_win_first = x - radius - 1;  // add right, remove left
            x_win_last = x + radius;
            blur_window_add_and_remove(&win, _pixel_ptr(src, x_win_last, y), _pixel_ptr(src, x_win_first, y));
            apply_blur(&win, _pixel_ptr(dest, x, y));
        }

        // Phase 3: shrinking window (right edge), x = [x_end-radius .. x_end-1]
        for (; x < x_end; x++) {
            x_win_first = x - radius - 1;
            if (x_win_first >= x_start) {
                blur_window_remove(&win, _pixel_ptr(src, x_win_first, y));
            }
            apply_blur(&win, _pixel_ptr(dest, x, y));
        }
    }
}



static inline void blur_window_box_algorithm_ver(gbuffer *src, gbuffer *dest, garea rect, int radius, blur_window_apply_func apply_blur) {
    // this method mirrors the same for vertical
    blur_window win;
    uint32_t *dest_pix;

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
            if (yi >= y_start) {
                blur_window_add(&win, _pixel_ptr(src, x, yi));
            }
        }

        // Phase 1: growing window (left edge),  x = [x_start .. x_start+radius-1]
        for (y = y_start; y < y_start + radius && y < y_end; y++) {
            y_win_last = y + radius;
            if (y_win_last < y_end) {
                blur_window_add(&win, _pixel_ptr(src, x, y_win_last));
            }
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
            if (y_win_first >= y_start) {
                blur_window_remove(&win, _pixel_ptr(src, x, y_win_first));
            }
            apply_blur(&win, _pixel_ptr(dest, x, y));
        }
    }
}

