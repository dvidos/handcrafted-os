#include "ui_style.h"
#include "../graphics/fonts/font8x16.h"

static ui_style_t global_ui_style;


void initialize_ui_style() {
    // we have the option to initialize things here, 
    // e.g. derive colors based on contrast / Hue-Saturation-Value / etc.
    font8x16 *base_font = geneva9;
    float contrast = 0.25;
    color base_gray = 0xffaaaaaa;
    color base_text = 0xff333333;
    color selected = color_with_alpha(0x7F, color_nextstep_bg());

    global_ui_style = (ui_style_t){
        .wallpaper = {
            .fill = fill_params_solid(color_nextstep_bg()),
        },
        .window = {
            .bg = fill_params_solid(base_gray),
            .text = text_params_of(base_font, ALIGN_MIDDLE_LEFT, base_text),
            .light = color_darken(base_gray, contrast),
            .dark = color_darken(base_gray, contrast),
            .padding = 16,
            .spacing = 12,
        },
        .control = {
            .bg = fill_params_solid(base_gray),
            .text = text_params_of(base_font, ALIGN_MIDDLE_LEFT, base_text),
            .input_bg = fill_params_solid(color_gray_of(0xcc)),
            .light = color_darken(base_gray, contrast),
            .dark = color_darken(base_gray, contrast),
            .border_color = base_text,
            .border_style = BORDER_FLAT,
            .contrast_3d = 0.5f,
            .border_thickness = 1,
            .height = 21,
            .button_min_width = 80,
        },
        .menus = {
            .menu = {
                // .bg = fill_params_gradient(base_gray, color_darken(base_gray, 0.2f), point_zero(), point_of(0, 200), ease_linear),
                .bg = fill_params_solid(base_gray),
                .border = border_params_of(BORDER_RAISED, base_gray, 2, 5, 0.3f),
                .padding = size_of(4, 4),
            },
            .item = {
                .bg = fill_params_none(),
                .text = text_params_of(base_font, ALIGN_MIDDLE_LEFT, base_text),
                .bg_selected = fill_params_solid(selected),
                .text_selected = text_params_of(base_font, ALIGN_MIDDLE_LEFT, color_white()),
                .padding = size_of(8, 8),
            },
            .min_item_width = 75,
        },
    };
};

const ui_style_t *ui_style() {
    return &global_ui_style;
}
