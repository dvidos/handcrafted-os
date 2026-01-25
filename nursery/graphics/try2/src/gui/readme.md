# gui

Here, to re-structure the GUI as a library that can be used by (a) a kernel or (b) a stand alone program.

Dependencies should contain:

* memory allocator, 
* returned error codes, 
* a clock device (get ticks, get timestamp),
* a logger interface,
* internal string implementations, if compiling standalone

From then on, we should not care if it's a 32 bit kernel using us or a x64 hosted process.

The entry point would be something like: "gui_handle_event(gui_event e)"
