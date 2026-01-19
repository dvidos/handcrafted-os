#include "../app_kit/layout_helper.h"
#include "../app_kit/views/text_view.h"
#include "../concepts/screen_manager.h"
#include "../app_kit/ui_style.h"
#include "../graphics/fonts/font8x16.h"
#include "menus.h"

typedef struct menu_runtime {
    menu_t *menu;
    bool is_popup;
    surface_t *events_recipient;
    int selection; // -1 means none
    size item_size;
    size menu_size;
    const struct menu_style *menu_style;
} menu_runtime_t;

static menu_runtime_t *_create_menu_runtime(menu_t *m, bool is_popup, surface_t *events_recipient) {
    const struct menu_style *ms = &ui_style()->menu;
    
    int widest_item = ms->min_item_width;
    int item_height = 0;
    int total_height = 0;

    for (int i = 0; i < m->count; i++) {
        menu_item_t *mi = &m->items[i];
        size caption_size = font8x16_get_text_size(ms->item_text.font, mi->caption);
        int item_width = caption_size.width + ms->item_padding.width * 2;
        widest_item = caption_size.width > widest_item ? caption_size.width : widest_item;
        item_height = caption_size.height + ms->item_padding.height * 2;
        total_height += item_height;
    }
    
    menu_runtime_t *mr = kmalloc(sizeof(menu_runtime_t));
    mr->menu = m;
    mr->is_popup = is_popup;
    mr->events_recipient = events_recipient;
    mr->menu_style = ms;
    mr->item_size = size_of(widest_item, item_height);
    mr->menu_size = size_of(widest_item + ms->menu_padding.width * 2, total_height + ms->menu_padding.height * 2);
    mr->selection = 0; // first one selected by definition

    return mr;
}

static void destroy_menu_runtime(menu_runtime_t *mr) {
    kfree(mr);
}

static inline area _get_item_area(menu_runtime_t *mr, int index) {
    return area_of(
        mr->menu_style->menu_padding.width,
        mr->menu_style->menu_padding.height + index * mr->item_size.height,
        mr->item_size.width,
        mr->item_size.height
    );
}

static void inline _change_selection(surface_t *s, menu_runtime_t *mr, int new_selection) {
    area old_sel_area = _get_item_area(mr, mr->selection);
    mr->selection = new_selection;
    area new_sel_area = _get_item_area(mr, mr->selection);
    surface_invalidate_area(s, area_union(old_sel_area, new_sel_area));
    
}

static void inline _enqueue_menu_event(surface_t *s, menu_runtime_t *mr, int selection) {
    event_t e = (event_t){
        .type = EVT_TARGETED,
        .targeted = (targeted_event_t){
            .target = mr->events_recipient,
            .type = (selection == -1) ? EVT_MENU_CANCELED : EVT_MENU_SELECTED,
            .event_arg = mr->menu->items[selection].id,
        }
    };
    screen_manager_enqueue_event(e);
}


static void inline _vanish_if_transient(surface_t *s, menu_runtime_t *mr) {
    if (mr->is_popup)
        screen_manager_remove_surface(s);
}

static void _handle_key(surface_t *s, key_event_t e) {
    menu_runtime_t *mr = surface_get_client_data(s);
    int new_selection;

    // handle arrows, enter, esc, possibly shortcuts
    if (e.type != KEY_PRESSED || e.keymods != 0) return;

    switch (e.keycode) {
        case KEY_ENTER:
            if (mr->selection >= 0 && mr->selection < mr->menu->count) {
                _enqueue_menu_event(s, mr, mr->selection);
                _vanish_if_transient(s, mr);
            }
            break;

        case KEY_ESCAPE:
            _enqueue_menu_event(s, mr, -1);
            _vanish_if_transient(s, mr);
            break;

        case KEY_UP:
            new_selection = mr->selection == 0 ? mr->menu->count - 1 : mr->selection - 1;
            _change_selection(s, mr, new_selection);
            break;
        case KEY_DOWN:
            new_selection = mr->selection == mr->menu->count - 1 ? 0 : mr->selection + 1;
            _change_selection(s, mr, new_selection);
            break;
    }
}

static int _find_selection_under_mouse(menu_runtime_t *mr, point mouse_pos) {
    int mouse_selection = -1;
    for (int i = 0; i < mr->menu->count; i++) {
        area item_area = _get_item_area(mr, i);
        if (area_contains(item_area, mouse_pos))
            return i;
    }
    return -1;
}

static void _handle_mouse(surface_t *s, mouse_event_t e) {
    menu_runtime_t *mr = surface_get_client_data(s);

    if (e.type == MOUSE_MOVED) {
        int mouse_selection = _find_selection_under_mouse(mr, e.pos);
        if (mr->selection != mouse_selection)
            _change_selection(s, mr, mouse_selection);
    } else if (e.type == MOUSE_LBTN_DOWN) {
        int mouse_selection = _find_selection_under_mouse(mr, e.pos);
        // only cancel if we are is_popup
        if (mr->is_popup) {
            _enqueue_menu_event(s, mr, mouse_selection);
            _vanish_if_transient(s, mr);
        }
    }
}

static void _paint_item(menu_runtime_t *mr, graphics_context_t *gc, int index, area clip) {
    area item_area = _get_item_area(mr, index);
    if (area_is_empty(area_intersect(item_area, clip)))
        return;

    gc_set_fill(gc, (index == mr->selection) ? mr->menu_style->item_bg_selected : mr->menu_style->item_bg);
    gc_draw_rect(gc, item_area);

    gc_set_text(gc, (index == mr->selection) ? mr->menu_style->item_text_selected : mr->menu_style->item_text);
    area text_area = area_grow(item_area, -mr->menu_style->item_padding.width, -mr->menu_style->item_padding.height);
    gc_draw_text(gc, mr->menu->items[index].caption, text_area);

    // possibly submenu indicator?
    // possible hot key? checkmark? etc
}

static void _paint(surface_t *s, graphics_context_t *gc, area dirty) {
    menu_runtime_t *mr = (menu_runtime_t *)surface_get_client_data(s);

    gc_set_fill(gc, mr->menu_style->menu_bg);
    gc_draw_rect(gc, area_of(0, 0, mr->menu_size.width, mr->menu_size.height));

    for (int i = 0; i < mr->menu->count; i++) {
        _paint_item(mr, gc, i, dirty);
    }
}

// -------------------------------------------------------------------------------------

surface_t *create_vertical_menu_surface(menu_t *m, bool is_popup, surface_t *events_recipient) {
    menu_runtime_t *mr = _create_menu_runtime(m, is_popup, events_recipient);

    surface_t *s = new_surface(mr->menu_size.width, mr->menu_size.height, SURFACE_MENU, true, "vertical_menu");
    surface_set_client_data(s, mr);
    surface_set_key_handler(s, _handle_key);
    surface_set_mouse_handler(s, _handle_mouse);
    surface_set_on_paint_behavior(s, _paint);

    return s;
}
