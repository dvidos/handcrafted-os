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
        struct {
            fill_params bg;
            border_params border;
            size padding;
        } menu;
        struct {
            fill_params bg;
            size padding;
            text_params text;
            fill_params bg_selected;
            text_params text_selected;
        } item;
        int min_item_width;
    } menus;

} ui_style_t;


void initialize_ui_style();
const ui_style_t *ui_style();
