#pragma once


#define TCGETS      0x5401  // Get current terminal settings (into struct termios)
#define TCSETS      0x5402  // Set terminal settings immediately
#define TCSETSW     0x5403  // Set settings after waiting for output to drain
#define TCSETSF     0x5404  // Set settings after draining output and flushing input

#define TIOCGWINSZ  0x5413  // Get Window Size: returns rows, cols, pixels (struct winsize)
#define TIOCSWINSZ  0x5414  // Set Window Size: used by terminal emulators to tell the kernel the window resized

#define TIOCGPGRP   0x540F  // Get Process Group: find out which process group is in the foreground
#define TIOCSPGRP   0x5410  // Set Process Group: make a specific process group the foreground (e.g., when you run a command in sash)

#define TIOCSCTTY   0x540E  // Set Controlling TTY: make this TTY the controlling terminal for the calling process
#define FIONREAD    0x541B  // Get the number of bytes currently available to read in the input buffer
#define TIOCFLUSH   0x540B  // Flush (discard) all data in the input or output queues

