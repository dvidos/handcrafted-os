#include "viewport.h"

typedef struct viewport_priv_data {
    // nothing for now
} viewport_priv_data_t;


static int viewport_repaint(viewport_t *vp) {
    // repaint on screen
}

static int viewport_ensure_cursor_visible(viewport_t *vp) {
    // re-validate offsets
    // repaint if needed
}

viewport_t *create_multi_line_buffer_viewport(buffer_t *buffer) {
    viewport_t *vp = malloc(sizeof(viewport_t);
    memset(vp, 0, sizeof(viewport_t));
    vp->buffer = buffer;

    viewport_priv_data_t *priv_data = malloc(sizeof(viewport_priv_data_t));
    vp->priv_data = priv_data;

    viewport_ops_t *ops = malloc(sizeof(viewport_ops_t));
    ops->repaint = viewport_repaint;
    ops->ensure_cursor_visible = viewport_ensure_cursor_visible;
    vp->ops = ops;

    return vp;
}

void destroy_multi_line_buffer_viewport(viewport_t *vp) {
    if (vp->ops)
        free(vp->ops);
    free(vp);
}

