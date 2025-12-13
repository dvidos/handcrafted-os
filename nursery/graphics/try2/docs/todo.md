# possible future todos

* one small bitmap font for primitive text rendering (maybe embedded in code, i.e. macos style)
* some primitive rectangle & filled_rectangle functions

* try the scalable font on osdev.org, or [here](https://gitlab.com/bztsrc/scalable-font2)
* try shadeable rectangles with rounded corners, ala blackbox


## base graphics engine

* compiled in kernel
* basic rectangles & frames, 
* basic text
* maybe 8-16 hardcoded colors by name (e.g. red, green, blue, yellow, orange, brown, light, dark, and 5 shades of gray)
* to be able to achieve a very bare bones thing, way before [this one](https://applemuseum.bott.org/sections/images/screenshots/system1/desktop.gif)

## advanced graphics engine

* scalable fonts, antialiased, w/shadow
* shadows
* rounded corners, antialiased
* gradients
* alpha / opacity
* blur (for panes / icons etc)
* emulated 3d appearance (sunken, raised) with definable curature, height/depth, and distance from the edge
* show images? icons? at least one format? (e.g. png, webp etc)

Essentially something to make as non-boxy graphics as possible, with as simple an interface as possible.
[Example openbox video](https://www.youtube.com/watch?v=5XoHWbdVhPc)

