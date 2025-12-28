# Graphical UI

This attempt was mainly driven to see if i can get graphics mode for my OS.
It turns out i can. But now what?


It seems a graphical interface is more challenging than a simple text one.
It definitely lends itself to object orientation at times, especially 
with "views" (as NextStep used to call them, or Widgets in Windows).

The high level components will be the following:

* Screen Manager
* Window Manager
* Process Manager

## Screen Manager

It owns the screen and what reaches it. It maintains a list
of z-ordered "surfaces", that composes together into
a back buffer and copies it on the hardware framebuffer.

Also owns the mouse and the text insertion cursor, 
paints them, tracks movement etc.

### surface_t

Owns a graphics buffer, and also has screen coordinates.

In order to write to it, a graphics context is created.

Surfaces hold views. Can be owned by windows, or other 
OS constructs. Examples of views are all app windows,
the alt-tab switcher widget, the dock and clock area,
the popup with the language or volume selection.

### view_t

A view is the intersection of specific graphic representation
(e.g. a raised rectangle with text) and a application-wise
useful functionality (e.g. trigger a command or edit some text)

From the perspective of the app, it consists of discrete classes
(e.g. `text_box_view_t`, `button_view_t` etc). But to outsiders,
they are just a `view_t`, the base and uniform interface, where 
the drawing and event delegation happens.

```c
struct view_t {
    void (*draw)();
    void (*on_event)(event_t *e);
}
struct text_box_t {
    view view;
    char *text;
    int position;
}
struct button_view_t {
    view view;
    char *caption;
    int id;
}
```

In order for views to appear on screen, they mark themselves
as dirty. Then, when the screen manager needs to compose the 
screen, it calls `paint()` with any dirty rectangles, 
so that the views paint only what's needed.

A view has many children and will do a `hit_test()` to find
the child that should be given the mouse event.

### graphics_context_t

A temporary tool through which to write to a graphical buffer.
It stores state (e.g. color, stroke width etc) that is used.
Methods can include:

```c
void push_state()
void pop_state()
void set_stroke_color()
void set_fill_pattern()
void draw_line()
void draw_border()
void draw_rect()
void draw_text()
```

### graphics_buffer_t

The actual collection of AARRGGBB pixels, and methods
of drawing graphical artifacts
(e.g. rects, borders, gradients, rounded rectangles, etc)

They don't have positional information. They are worked in memory.

Graphics buffers are copied and blended using alpha channel.
It all ends up a big graphics buffer called the `backbuffer`, 
which is then copied to the hardware framebuffer address.

### performance

Of course, performance is important. Big part of the solution
is tracking dirty rectangles and redrawing and copying only
those rectangles. 

### geometry

We have a collection of tools to help with drawing:

```c
struct point { int x, int y }
struct size { int width, int height}
struct vector { int dx, int dy }
struct area { int x, int y, int width, int height }
```

## Window Manager

This singleton owns the ordered, doubly-linked list
of windows, in z-order.

It can perform a hit-test to determine which window
was hit and also, which part of the window was hit.

### window_t

A structure that owns a surface (so that it can be shown on screen),
some decorations, the various flags (selectable, resizable,
moveable, closeable etc) and call back methods for handling events.

## Process Manager

It tracks and owns app lifetimes. 
It is responsible with associating windows with application instances.
I becomes the process model. 

I have not reached this point yet, but could simulate cooperating
multitasking by creating a process as the below, where i'll call
the collection in round robing fashion.

```c
struct ui_task {
    const char *name;
    struct ui_view *root_view;
    void (*tick)(void);
};
```

