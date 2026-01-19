#include "../app_kit/surface.h"
#pragma once

// modal menus to be asked by screen manager to start working, when selected, it's one more event.
// persistent menus (palettes, docks, etc) will be visible all time, clicking delivers events, but does not disappear
// both take a menu structure

typedef struct menu_item menu_item_t;
typedef struct menu menu_t;

struct menu_item {
    // later add hotkey, icon, disabled, separators, etc
    int id;
    const char *caption;
    menu_t *submenu; // usually null
};

struct menu {
    // later add: display as: icons, strings, max-visible-items (scrolling) etc.
    const char *debug_info;
    int count; // or the last one is null
    menu_item_t *items;
};

static inline menu_item_t menu_item_of(int id, const char *caption) { return (menu_item_t){.id = id, .caption = caption, .submenu = 0}; }


