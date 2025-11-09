[bits 64]

section .note.GNU-stack noalloc noexec nowrite progbits
section .stivale2hdr
	dq _start
	dq stack_top
	dq 0
	dq framebuffer

section .stivale2tags
	smp:
		dq 0x1ab015085f3273df
		dq 0
		dq 0
	framebuffer:
		dq 0x3ecc1bc43d0f7971
		dq smp
		dw 0
		dw 0
		dw 0

section .rodata
	align 16
		gdt_ptr:
			dw gdt_end - gdt_start - 1
			dq gdt_start		

	align 16
	gdt_start:
		.null_descriptor: ; 0x0
			dw 0xFFFF
			dw 0x0000
			db 0x00
			db 00000000b
			db 00000000b
			db 0x00
			
		.kernel_code_64: ; 0x8 K CS
			dw 0xFFFF
			dw 0x0000
			db 0x00
			db 10011010b
			db 10100000b
			db 0x00

		.kernel_data: ; 0x10 K SS
			dw 0xFFFF
			dw 0x0000
			db 0x00
			db 10010010b
			db 11000000b
			db 0x00
		.user_code_64: ; 0x1B CS
			dw 0xFFFF
			dw 0x0000
			db 0x00
			db 11111010b
			db 10100000b
			db 0x00
		.user_data_64: ; 0x23 SS
			dw 0xFFFF
			dw 0x0000
			db 0x00
			db 11110010b
			db 11000000b
			db 0x00
		.tss_64:
			dq 0x0
			dq 0x0
	
	align 16
	gdt_end:

section .bss
	stack_bottom:
		resb 16384
	stack_top:

section .text
	[global _start]
	_start:
		cli
		lgdt [gdt_ptr]

		mov rbp, rsp

		push 0x10
		push rbp
		pushfq
		push 0x8
		push startup64

		iretq

	[global smp64_start]
	smp64_start:
		cli
		lgdt [gdt_ptr]

		mov rbp, rsp

		push 0x10
		push rbp
		pushfq
		push 0x08
		push smp64_end

		iretq

	[extern processorEntry]
	smp64_end:
		mov ax, 0x10
		mov ds, ax
		mov es, ax
		mov fs, ax
		mov gs, ax
		mov ss, ax

		jmp processorEntry	

	[extern arch_entry]
	startup64:
		mov ax, 0x10
		mov ds, ax
		mov es, ax
		mov fs, ax
		mov gs, ax
		mov ss, ax

		jmp arch_entry