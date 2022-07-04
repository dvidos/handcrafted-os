#include <stdio.h>
#include <stdio.h>
#include <syscall.h>

#if defined(__is_libk)
// #include <kernel/tty.h>
#endif

int putchar(int c) {
#if defined(__is_libk)
	// char c = (char) ic;
	// terminal_write(&c, sizeof(c));
	((void)c);
	return -1;
#else
	return syscall(SYS_PUTCHAR, c, 0, 0, 0, 0);
#endif
}
