#pragma once
#include "../graphics/graphics.h"
#include "../graphics/color.h"
#include "../graphics/fonts/font8x16.h"


typedef struct {
    struct wallpaper_style {
        fill_params fill;
    } wallpaper;

    struct window_style {
        fill_params bg;
        text_params text;
        color dark;
        color light;
        int padding; // within root views etc
        int spacing; // between controls
    } window;

    struct control_style {
        fill_params bg;
        text_params text;
        fill_params input_bg; // for input text boxes etc.
        color dark;
        color light;
        color border_color;
        border_style_t border_style;
        float contrast_3d;
        int border_thickness;
        int height;
        int button_min_width;
    } control;

    struct menu_style {
        fill_params menu_bg; // there may be padding or separators
        fill_params item_bg;
        text_params item_text;
        fill_params item_bg_selected;
        text_params item_text_selected;
        size item_padding;
        size menu_padding;
        int min_item_width;
    } menu;

} ui_style_t;


void initialize_ui_style();
const ui_style_t *ui_style();
