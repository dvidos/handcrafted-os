#pragma once
#include "../fundamentals.h"
#include "keycodes.h"
#include "../graphics/geometry.h"


typedef enum {
    KEY_UP = 0,
    KEY_DOWN = 1
} key_event_type_t;

#define KEY_MOD_SHIFT  (1 << 0)
#define KEY_MOD_CTRL   (1 << 1)
#define KEY_MOD_ALT    (1 << 2)
#define KEY_MOD_SUPER  (1 << 3)

typedef struct key_event {
    key_event_type_t type;
    keycode_t keycode;     // logical key (layout-independent)
    uint8_t  modifiers;   // bitmask: shift/ctrl/alt/super
    char ascii;           // if printable ascii
} key_event_t;

#define MOUSE_BTN_LEFT   (1 << 0)
#define MOUSE_BTN_RIGHT  (1 << 1)
#define MOUSE_BTN_MIDDLE (1 << 2)

typedef enum mouse_event_type {
    MOUSE_MOVED,
    MOUSE_LBTN_DOWN,
    MOUSE_LBTN_UP,
    MOUSE_MBTN_DOWN,
    MOUSE_MBTN_UP,
    MOUSE_RBTN_DOWN,
    MOUSE_RBTN_UP,
    MOUSE_WHL_SCROLL
} mouse_event_type;

typedef struct mouse_event {
    mouse_event_type type;
    point pos;
    vector delta;
    uint8_t buttons;  // bitmask: L=1, R=2, M=4
    int8_t wheel_delta;  // signed change
} mouse_event_t;

typedef enum event_type {
    EVT_KEY,
    EVT_MOUSE
} event_type_t;

typedef struct event {
    event_type_t type;
    uint32_t     timestamp;
    union {
        key_event_t   key;
        mouse_event_t mouse;
    };
} event_t;

#define EVENT_QUEUE_SIZE 64

typedef struct event_queue {
    event_t events[EVENT_QUEUE_SIZE];
    uint32_t head;
    uint32_t tail;
} event_queue_t;

extern event_queue_t global_event_queue;

// -----------------------------------------


static inline int event_queue_empty(event_queue_t* q) { return q->head == q->tail; }
static inline int event_queue_full(event_queue_t* q) { return ((q->head + 1) % EVENT_QUEUE_SIZE) == q->tail; }

int event_queue_push(event_queue_t* q, const event_t* ev);
int event_queue_pop(event_queue_t* q, event_t* out);

void enqueue_key_event(const key_event_t *event);
void enqueue_mouse_event(const mouse_event_t *event);

void log_event_as_info(char *message, event_t *e);

static inline mouse_event_t mouse_event_localized(mouse_event_t e, area container) {
    mouse_event_t localized = e;  // copy values
    localized.pos = point_to_local(localized.pos, container);
    return localized;
}
