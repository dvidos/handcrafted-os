#pragma once

// how app could / should behave:

// this struct is the source of truth, or domain model.
typedef struct {
    char filter[64];
    bool show_archived;
    int selected_id;

    bool can_delete;
    bool loading;
} AppState;

// events flow from widgets to the app, through the bindings
typedef enum {
    EV_FILTER_CHANGED,
    EV_SHOW_ARCHIVED_CHANGED,
    EV_SELECT_ITEM,
    EV_DELETE_REQUESTED,
    EV_DATA_LOADED
} EventType;

typedef struct {
    EventType type;
    union {
        const char *s;
        bool b;
        int i;
    };
} Event;

// the bindings "set" method does something like the below:
void string_binding_set(string_binding_t *b, const char *v) {
    Event e = { .type = b->event_type, .s = v };
    dispatch(e);
}

// dispatch() is:
void dispatch(Event e) {
    update(&app_state, e);
    render(&app_state);
}

// update() is one central code, with all the behavior of the app in one place.
void update(AppState *state, Event e) {
    switch (e.type) {

    case EV_FILTER_CHANGED:
        strncpy(state->filter, e.s, sizeof(state->filter));
        break;

    case EV_SHOW_ARCHIVED_CHANGED:
        state->show_archived = e.b;
        break;

    case EV_SELECT_ITEM:
        state->selected_id = e.i;
        state->can_delete = (e.i >= 0);
        break;

    case EV_DELETE_REQUESTED:
        if (state->can_delete) {
            state->loading = true;
            start_async_delete(state->selected_id);
        }
        break;

    case EV_DATA_LOADED:
        state->loading = false;
        break;
    }
}

// and then, render() takes the info back to the UI:
void render(AppState *s) {
    set_text(filter_textbox, s->filter);
    set_checked(archived_checkbox, s->show_archived);

    set_enabled(delete_button, s->can_delete);
    set_visible(spinner, s->loading);
}
