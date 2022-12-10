/* For syntax on this file see
   https://sourceware.org/binutils/docs/ld/Input-Section.html and
   https://home.cs.colorado.edu/~main/cs1300/doc/gnu/ld_3.html
*/

/* define the first 64K for memory for simplicity */
MEMORY {
	first64k : ORIGIN = 0x1000, LENGTH = 0xEFFF
}



/* make sure what the first file will be */
ENTRY(start_asm)
/* STARTUP(start.o) /* but this loads it twice... */

/* Tell where the various sections of the object files will be put in the final
   kernel image. */
SECTIONS
{
	. = 0x1000;
	all : {

		/* code. sort of needed */
		*(.text)

		/* initialized data, has to describe values */
		*(.data)     

		/* uninitialized data, 
		   if not included, they are mapped to be at the end of the image (e.g. 0x5000+)
		   let's include for now, grows the bootloader size, 
		      but allows clear boundaries, so we can initialize the heap
		*/
		*(.bss)

		/* mainly strings */
		*(.rodata)   

		/* like "GCC: (GNU) 11.2.0" */
		/*  *(.comment)  */

		FILL(0x00);
		. = 0x1000 + 32 * 512;  /* pad output to specific number of sectors (16k) */
	} >first64k
}

OUTPUT_FORMAT(binary)
