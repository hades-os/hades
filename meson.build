project('hades', 'cpp', version: '0.0.1')
main_sources = files(
    'source/cxx/sys/x86/apic.cpp',
    'source/cxx/sys/acpi.cpp',
	'source/cxx/sys/pci.cpp',
	'source/cxx/sys/smp.cpp',
	'source/cxx/sys/irq.cpp',
	'source/cxx/sys/sched.cpp',
	'source/cxx/sys/runtime.cpp',
    'source/cxx/entry.cpp',
    'source/cxx/mm/ctx.cpp',
	'source/cxx/mm/mm.cpp',
    'source/cxx/mm/pmm.cpp',
    'source/cxx/mm/vmm.cpp',
	'source/cxx/fs/vfs.cpp',
	'source/cxx/fs/rootfs.cpp',
	'source/cxx/fs/dev.cpp',
	'source/cxx/fs/fat.cpp',
	'source/cxx/driver/ahci.cpp',
	'source/cxx/driver/part.cpp'
)
main_includes = include_directories('include')
nasm_sources = files(
    'source/asm/boot.asm',
    'source/asm/irq.asm'
)

nasm = find_program('nasm')
nasm_gen = generator(nasm, output: '@BASENAME@.o', arguments: ['-f', 'elf64', '-g', '-F', 'dwarf', '@INPUT@', '-o', '@OUTPUT@'])
nasm_objects = nasm_gen.process(nasm_sources)

source_dir = join_paths(meson.source_root(), 'source')

ld_script = join_paths(source_dir, 'misc', 'linker.ld')

flags_cpp = [
    '-std=gnu++20',
	'-I$(INCLUDE)',
	'-ggdb',
	'-O0',
	'-ffreestanding',
	'-fdiagnostics-color=always',
	'-fno-stack-protector',
	'-fno-omit-frame-pointer',
	'-fno-rtti',
	'-fno-exceptions',
	'-fno-use-cxa-atexit',
	'-Wno-int-to-pointer-cast',
	'-fno-pic',
	'-mno-sse',
	'-mno-sse2',
	'-mno-mmx',
	'-mno-80387',
	'-mno-red-zone',
	'-m64',
	'-march=x86-64',
	'-mcmodel=kernel'
]

flags_ld = [
	'-T', ld_script,
    '-nostdlib',
	'-Wl,-z,max-page-size=0x1000',
	'-no-pie'
]

make_elf = executable('hades.elf', main_sources, nasm_objects, include_directories: main_includes, link_depends: ld_script, install: true, cpp_args: flags_cpp, link_args: flags_ld, build_by_default: false)

script_path = join_paths(meson.source_root(), 'scripts', 'post.py')
make_hades = custom_target(
	'hades',
	depends: make_elf,
	input: make_elf,
	output: '_',
	command: [script_path,
		meson.project_name(), 
		'-k', make_elf,
		
		'-s', join_paths(meson.source_root(), 'source'),
		'-b', meson.build_root(),
	],
	build_by_default: true
)