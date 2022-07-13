#include <stdio.h>
#include <syscall.h>


int puts(const char* string) {
	#ifdef __is_libk
		// use driver to write directly to screen
		((void)string);
		return -1;
	#endif
	#if __is_libc
	 	return syscall(SYS_PUTCHAR, (int)string, 0, 0, 0, 0);
	#endif
}
