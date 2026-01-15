#pragma once
#include "../graphics/graphics.h"
#include "../graphics/color.h"
#include "../graphics/fonts/font8x16.h"


typedef struct {
    struct wallpaper {
        fill_params fill;
    } wallpaper;

    struct window {
        fill_params bg;
        text_params text;
        color dark;
        color light;
        int padding; // within root views etc
        int spacing; // between controls
    } window;

    struct control {
        fill_params bg;
        text_params text;
        fill_params input_bg; // for input text boxes etc.
        color dark;
        color light;
        color border_color;
        border_style_t border_style;
        int border_thickness;
        int height;
        int button_min_width;
    } control;

    struct menu {
        fill_params bg;
        text_params text;
    } menu;

} ui_style_t;


void initialize_ui_style();
const ui_style_t *ui_style();
