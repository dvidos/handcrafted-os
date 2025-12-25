#include "../memory/sprintf.h"
#include "../memory/string.h"
#include "events.h"
#include "logger.h"


event_queue_t global_event_queue;

void log_event_as_info(char *message, event_t *e) {
    char buffer[256] = {0,};
    char *ptr = buffer;
    int len = sizeof(buffer);

    if (message != 0) {
        sprintfn(ptr, len, "%s: ", message);
        len -= strlen(ptr);
        ptr += strlen(ptr);
    }
    
    if (e->type == EVT_KEY) {
        sprintfn(ptr, len, "keycode %d, modifiers %d, ascii %c", e->key.keycode, e->key.modifiers, e->key.ascii);
    } else if (e->type == EVT_MOUSE) {
        sprintfn(ptr, len, "mouse at (%d,%d), delta (%d,%d), buttons %d, wheel %d", e->mouse.x, e->mouse.y, e->mouse.dx, e->mouse.dy, e->mouse.buttons, e->mouse.wheel);
    }
    len -= strlen(ptr);
    ptr += strlen(ptr);

    log.info(buffer);
}

int event_queue_push(event_queue_t* q, const event_t* ev) {
    if (event_queue_full(q)) {
        log.warn("event_queue_push() -- queue full, event dropped!");
        return 0; // drop or overwrite, your policy
    }

    // log.debug("event_queue_push()ing");
    q->events[q->head] = *ev; // copy struct contents
    q->head = (q->head + 1) % EVENT_QUEUE_SIZE;
    return 1;
}

int event_queue_pop(event_queue_t* q, event_t* out) {
    if (event_queue_empty(q))
        return 0;

    // log.debug("event_queue_pop()ing");
    *out = q->events[q->tail]; // copy struct contents
    q->tail = (q->tail + 1) % EVENT_QUEUE_SIZE;
    return 1;
}

void enqueue_key_event(const key_event_t *event) {
    // log.debug("enqueue_key_event()");
    event_t e;
    e.type = EVT_KEY;
    e.timestamp = 0;
    e.key = *event;
    event_queue_push(&global_event_queue, &e);
}

void enqueue_mouse_event(const mouse_event_t *event) {
    // log.debug("enqueue_mouse_event()");
    event_t e;
    e.type = EVT_MOUSE;
    e.timestamp = 0;
    e.mouse = *event;
    event_queue_push(&global_event_queue, &e);
}

