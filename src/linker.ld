/* The bootloader will look at this image and start execution at the symbol
   designated as the entry point. */
OUTPUT_FORMAT(elf32-i386)

/* Tell where the various sections of the object files will be put in the final
   kernel image. */
SECTIONS
{
    /* put early init code here */
    . = 0x100000;
    .multiboot : AT( 0 ) {
        *(.multiboot)
    }
    
    .entry ALIGN(4K) : AT( ADDR(.entry) ) {
        *(.entry)
        *(.boot_page_tables)
    }

	/* All other symbols should be linked to the kernel base address, but *placed* from 0x100000. */
	/* fixme: we map the kernel 1mb below where the linker expects it to go. */
	. = 0xC0105000;

	.text ALIGN(4K) : AT(ADDR(.text) - 0xC0000000)
	{
        kernel_start_phys = (. - 0xC0000000);
        kernel_start = .;
		*(.text)
        *(.rodata*)
	}

	/* Read-write data (initialized) */
	.data ALIGN(4K) : AT(ADDR(.data) - 0xC0000000)
	{
		*(.data)
	}

	/* Read-write data (uninitialized) and stack */
	.bss ALIGN(4K) : AT(ADDR(.bss) - 0xC0000000)
	{
        _sbss = .;
		*(COMMON)
		*(.bss)
		*(.bootstrap_stack)
        _ebss = .;
	}

	/* The compiler may produce other sections, by default it will put them in
	   a segment with the same name. Simply add stuff here as needed. */
   kernel_end = .;
   kernel_end_phys = (. - 0xC0000000);
}
__CTOR_LIST__ = ADDR(.ctors);
