/* For syntax on this file see
   https://sourceware.org/binutils/docs/ld/Input-Section.html and
   https://home.cs.colorado.edu/~main/cs1300/doc/gnu/ld_3.html
*/

/* The bootloader will look at this image and start execution at the symbol
   designated as the entry point. */
ENTRY(_start)

/* Tell where the various sections of the object files will be put in the final
   kernel image. */
SECTIONS
{
	/* KERNEL_LOAD_ADDRESS is passed in from the makefile */
	. = KERNEL_LOAD_ADDRESS;
    _linker_start_address = .;

	/* let's put start first, to ensure our jump will land there */
	_segment_text_start = .;
	.text BLOCK(4K) : ALIGN(4K)
	{
		*(.text.start)
	}
	.text BLOCK(4K) : ALIGN(4K)
	{
		*(.text)
	}
	_segment_text_end = .;

	/* Read-only data. */
	_segment_rodata_start = .;
	.rodata BLOCK(4K) : ALIGN(4K)
	{
		*(.rodata)
	}
	_segment_rodata_end = .;

	/* Read-write data (initialized) */
	_segment_init_data_start = .;
	.data BLOCK(4K) : ALIGN(4K)
	{
		*(.data)
	}
	_segment_init_data_end = .;

	/* Read-write data (uninitialized) and stack */
	_segment_zero_data_start = .;
	.bss BLOCK(4K) : ALIGN(4K)
	{
		*(COMMON)
		*(.bss)
	}
	_segment_zero_data_end = .;

	/* The compiler may produce other sections, by default it will put them in
	   a segment with the same name. Simply add stuff here as needed. */

	_linker_end_address = .;
}
