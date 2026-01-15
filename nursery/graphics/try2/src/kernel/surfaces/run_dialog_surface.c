#include "../app_kit/surface.h"
#include "../app_kit/layout_helper.h"
#include "../app_kit/views/textbox_view.h"
#include "../app_kit/views/text_view.h"
#include "../app_kit/views/button_view.h"
#include "../concepts/logger.h"
#include "../concepts/screen_manager.h"



static void _run_dialog_ok_clicked(void *userdata) {
    surface_t *s = (surface_t *)userdata;

    log.info("Run dialog OK clicked, should run the cmd line");
    // ?
    // well, maybe we need some custom struct, otherwise where are we going to hang our views?
    screen_manager_remove_surface(s);
}

static void _run_dialog_cancel_clicked(void *userdata) {
    surface_t *s = (surface_t *)userdata;

    log.info("Run dialog Cancel clicked, should just close the pane");
    screen_manager_remove_surface(s);
}

surface_t *create_run_dialog_surface() {
    const ui_style_t *style = ui_style();
    vert_layout_t vl = new_vert_layout(300, ui_style()->window.padding, ui_style()->window.spacing);

    surface_t *s = new_surface(500, 250, SURFACE_OVERLAY, true, "run-dialog");

    text_view *prompt = new_text_view("Enter command to run");
    textbox_view *text = new_textbox_view();
    button_view *ok = new_button_view("OK", _run_dialog_ok_clicked, s);
    button_view *cancel = new_button_view("Cancel", _run_dialog_cancel_clicked, s);

    surface_add_view(s, (view_t *)prompt);
    vert_layout_add(&vl, (view_t *)prompt, ui_style()->control.height);

    surface_add_view(s, (view_t *)text);
    vert_layout_add(&vl, (view_t *)text, ui_style()->control.height);

    surface_add_view(s, (view_t *)cancel);
    surface_add_view(s, (view_t *)ok);
    vert_layout_add_two_buttons(&vl, (view_t *)cancel, (view_t *)ok, ui_style()->control.button_min_width, ui_style()->control.height);

    area boundaries = vert_layout_boundaries(&vl);
    boundaries = screen_manager_center_on_screen(boundaries);

    surface_set_size(s, boundaries.width, boundaries.height);
    surface_set_position(s, boundaries.x, boundaries.y);

    return s;
}
