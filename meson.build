project('Trajan', 'cpp', version: '0.0.1')

main_sources = files(
    'source/sys/x86/apic.cpp',
    'source/sys/acpi.cpp',
	'source/sys/pci.cpp',
	'source/sys/smp.cpp',
	'source/sys/irq.cpp',
	'source/sys/sched.cpp',
	'source/sys/runtime.cpp',
    'source/entry.cpp',
    'source/mm/ctx.cpp',
	'source/mm/mm.cpp',
    'source/mm/pmm.cpp',
    'source/mm/vmm.cpp',
	'source/fs/vfs.cpp',
	'source/fs/rootfs.cpp',
	'source/fs/devfs.cpp',
	'source/driver/ahci.cpp',
	'source/driver/part.cpp'
)

cpp_includes = include_directories('include')

nasm_sources = files(
    'source/util/asm/boot.asm',
    'source/util/asm/irq.asm',
    'source/util/asm/lock.asm'
)

nasm = find_program('nasm')
nasm_gen = generator(nasm, output: '@BASENAME@.o', arguments: ['-f', 'elf64', '-g', '-F', 'dwarf', '@INPUT@', '-o', '@OUTPUT@'])
nasm_objects = nasm_gen.process(nasm_sources)

ld_script = meson.current_source_dir() + '/misc/linker.ld'

flags_cpp = [
    '-std=gnu++20',
	'-g3',
	'-Og',
	'-ffreestanding',
	'-I$(INCLUDE)',
	'-mno-sse',
	'-mno-sse2',
	'-mno-mmx',
	'-mno-80387',
	'-mno-red-zone',
	'-m64',
	'-march=x86-64',
	'-fdiagnostics-color=always',
	'-fno-stack-protector',
	'-fno-omit-frame-pointer',
	'-fno-merge-constants',
	'-fno-rtti',
	'-fno-exceptions',
	'-fno-use-cxa-atexit',
	'-Wno-pointer-arith',
	'-Wno-int-to-pointer-cast',
	'-Wno-volatile',
	'-fno-pic',
	'-mcmodel=kernel'
]

flags_ld = [
	'-T', ld_script,
    '-nostdlib',
	'-Wl,-z,max-page-size=0x1000',
	'-no-pie'
]

executable('trajan.elf', main_sources, nasm_objects, include_directories: cpp_includes, link_depends: ld_script, install: true, cpp_args: flags_cpp, link_args: flags_ld)