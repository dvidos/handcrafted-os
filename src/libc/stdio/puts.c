#include <stdio.h>
#include <syscall.h>


int puts(const char* string) {
	#ifdef __is_libk
		// use driver to write directly to screen
		((void)string);
		return -1;
	#elif __is_libc
	 	return syscall(SYS_PUTCHAR, string, 0, 0, 0, 0);
	#endif
}
