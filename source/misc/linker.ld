ENTRY (_start)

OUTPUT_FORMAT(elf64-x86-64)

SECTIONS { 
   . = 0xFFFFFFFF80100000;

   .stivale2hdr : ALIGN(4K) {
        *(.stivale2hdr)
   }

   .text : ALIGN(4K) {
        *(.text)
   }

   .init_array : ALIGN(8) {
		__init_array_begin = .;
		KEEP(*(SORT(.init_array.*)))
		KEEP(*(.init_array*))
		__init_array_end = .;
	}

    .fini_array : ALIGN(8) {
        __fini_array_begin = .;
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array))
        __fini_array_end = .;
    }

    .ex_table : ALIGN(8) {
        _ex_table_begin = .;
        KEEP(*(SORT(.ex_table.*)))
        KEEP(*(.ex_table))
        _ex_table_end = .;
    }

   .data : ALIGN(4K) {
   	    *(.data)
        *(.rodata)
        *(.stivale2tags)
   }

   .bss : ALIGN(4K) {
        *(COMMON)
	    *(.bss)

        . = ALIGN(4K);
        __boot_heap_start = .;
        . += 512K;
        __boot_heap_end = .;
   }
}
