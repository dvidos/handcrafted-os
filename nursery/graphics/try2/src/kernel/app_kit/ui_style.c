#include "ui_style.h"
#include "../graphics/fonts/font8x16.h"

static ui_style_t global_ui_style;


void initialize_ui_style() {
    // we have the option to initialize things here, 
    // e.g. derive colors based on contrast / Hue-Saturation-Value / etc.
    font8x16 *base_font = geneva9;
    float contrast = 0.5;
    color base_gray = 0xffaaaaaa;
    color base_text = 0xff333333;

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
            .text = text_params_of(base_font, ALIGN_MIDDLE_CENTER, base_text),
            .input_bg = fill_params_solid(color_lighten(base_gray, 0.85)),
            .light = color_darken(base_gray, contrast),
            .dark = color_darken(base_gray, contrast),
            .border_color = base_text,
            .border_thickness = 1,
            .height = 20,
            .button_min_width = 80,
        },
        .menu = {
            .bg = fill_params_solid(base_gray),
            .text = text_params_of(base_font, ALIGN_MIDDLE_LEFT, base_text),
        },
    };
};

const ui_style_t *ui_style() {
    return &global_ui_style;
}
