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
	/* Begin putting sections at 1 MB, a conventional place for kernels to be
	   loaded at by the bootloader. 100k works for loading on low memory as well */
	. = 1M;
    kernel_start_address = .;

	/* First put the multiboot header, as it is required to be put very early
	   early in the image or the bootloader won't recognize the file format.
	   Next we'll put the .text section. */
	.text BLOCK(4K) : ALIGN(4K)
	{
		*(.multiboot)
		*(.text)
	}
	kernel_text_end_address = .;

	/* Read-only data. */
	.rodata BLOCK(4K) : ALIGN(4K)
	{
		*(.rodata)
	}
	kernel_rodata_end_address = .;

	/* Read-write data (initialized) */
	.data BLOCK(4K) : ALIGN(4K)
	{
		*(.data)
	}
	kernel_data_end_address = .;

	/* Read-write data (uninitialized) and stack */
	.bss BLOCK(4K) : ALIGN(4K)
	{
		*(COMMON)
		*(.bss)
	}
	kernel_bss_end_address = .;

    kernel_text_size = SIZEOF(.text);
    kernel_rodata_size = SIZEOF(.rodata);
    kernel_data_size = SIZEOF(.data);
    kernel_bss_size = SIZEOF(.bss);

	/* The compiler may produce other sections, by default it will put them in
	   a segment with the same name. Simply add stuff here as needed. */

	kernel_end_address = .;
}
