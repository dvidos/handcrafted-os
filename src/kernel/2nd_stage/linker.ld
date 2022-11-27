/* For syntax on this file see
   https://sourceware.org/binutils/docs/ld/Input-Section.html and
   https://home.cs.colorado.edu/~main/cs1300/doc/gnu/ld_3.html
*/

/* ENTRY(_start) */
STARTUP(start.o)

/* Tell where the various sections of the object files will be put in the final
   kernel image. */
SECTIONS
{
	/* all my sections will be in 0x10000 (64kb) */
 	. = 0x10000;
	.text : { *(.text) }
	.rodata : { *(.rodata) }
	.data : { *(.data) }
  	.bss : { *(.bss) }	
}

OUTPUT_FORMAT(binary)
