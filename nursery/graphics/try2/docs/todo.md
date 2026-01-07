# next steps

1. ~~**Surfaces** - Make surface_t a first-class object (buffer, rect, z-order). Everything visible is a surface.~~
1. ~~**GraphicsContext** - Introduce a stateless drawing API that renders into a surface buffer.~~
1. **Base View** - Define view_t with draw, hit-test, and event handlers (single view, no hierarchy).
1. **Concrete Views** - Implement Button, Label, TextBox by embedding view_t and overriding behavior.
1. **View Hierarchy** - Add parent/children to views, recursive draw and hit-testing.
1. **Window** - Introduce window_t as a thin owner of a surface and a root view.
1. **Window Manager** - Manage window stacking, focus, movement, and event routing.
1. **System UI** - Implement desktop, menu bar, dock, and overlays as non-window surfaces with views.

Work for a complete-end-to-end system first, then a beautiful and performant system.

Also, in technology, checkboxes to tick:

* Shadows, with smaller footprint than the shadowed object
* Scalable fonts, with antialiasing
* Resolution independent, take idea from font system, work at points, not pixels (scaling in graphics_context)
* Blur for frosted glass look of terminals, menus etc.

Proof of concept work:

* UI top or bottom bar, with clock and icons
* Two windows, movable or tiled, full screenable or not,
* Menus, nestable, per window and popup menus
* Alt+Tab searcher or similar, with results list


## possible future todos

* ~~one small bitmap font for primitive text rendering (maybe embedded in code, i.e. macos style)~~
* ~~some primitive rectangle & filled_rectangle functions~~

* try the scalable font on osdev.org, or [here](https://gitlab.com/bztsrc/scalable-font2)
* ~~try shadeable rectangles with rounded corners, ala blackbox~~


## base graphics engine

* compiled in kernel
* ~~basic rectangles & frames,~~
* ~~basic text~~
* ~~maybe 8-16 hardcoded colors by name (e.g. red, green, blue, yellow, orange, brown, light, dark, and 5 shades of gray)~~
* to be able to achieve a very bare bones thing, way before [this one](https://applemuseum.bott.org/sections/images/screenshots/system1/desktop.gif)

## advanced graphics engine

* ~~rounded corners, antialiased~~
* scalable fonts, antialiased, w/shadow
* ~~shadows~~
* ~~gradients~~
* ~~alpha / opacity~~
* ~~blur (for panes / icons etc)~~
* emulated 3d appearance (sunken, raised) with definable curature, height/depth, and distance from the edge
* show images? icons? at least one format? (e.g. png, webp etc)

Essentially something to make as non-boxy graphics as possible, with as simple an interface as possible.
[Example openbox video](https://www.youtube.com/watch?v=5XoHWbdVhPc)


## crazy ideas

- I'd like cryptography and something like `pass` or `vault` to be built into the system.
- I'd like to have clean separation between the system files and the user files. 
User should not be able to see system files, unless they restart as a technician / admin 
(in which case they wouldn't see their files as a user)

