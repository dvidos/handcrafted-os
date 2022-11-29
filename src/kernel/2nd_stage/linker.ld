/* For syntax on this file see
   https://sourceware.org/binutils/docs/ld/Input-Section.html and
   https://home.cs.colorado.edu/~main/cs1300/doc/gnu/ld_3.html
*/

/* make sure what the first file will be */
STARTUP(start.o)

/* Tell where the various sections of the object files will be put in the final
   kernel image. */
SECTIONS
{
	all : {
		. = 0x0;
		*(.text)
		*(.rodata)
		*(.data)
		*(.bss)
		FILL(0x00);
		. = 4 * 512;  /* pad output to specific number of sectors */
	}
}

OUTPUT_FORMAT(binary)
