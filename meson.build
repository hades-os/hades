project('hades', ['cpp', 'c'], version: '0.0.1')
main_sources = files(
    'source/cxx/sys/x86/apic.cpp',
    'source/cxx/sys/acpi.cpp',
	'source/cxx/sys/pci.cpp',
	'source/cxx/sys/smp.cpp',
	'source/cxx/sys/irq.cpp',
	'source/cxx/sys/sched/sched.cpp',
    'source/cxx/sys/sched/wait.cpp',
    'source/cxx/sys/sched/signal.cpp',
    'source/cxx/sys/pit.cpp',
	'source/cxx/sys/runtime.cpp',
    'source/cxx/sys/syscall.cpp',
    'source/cxx/sys/laihost.cpp',
    'source/cxx/entry.cpp',
    'source/cxx/mm/ctx.cpp',
	'source/cxx/mm/mm.cpp',
    'source/cxx/mm/pmm.cpp',
    'source/cxx/mm/vmm.cpp',
	'source/cxx/fs/vfs.cpp',
	'source/cxx/fs/rootfs.cpp',
	'source/cxx/fs/dev.cpp',
	'source/cxx/fs/fat.cpp',
    'source/cxx/driver/tty/tty.cpp',
    'source/cxx/driver/tty/pty.cpp',
    'source/cxx/driver/tty/disciplines.cpp',
    'source/cxx/driver/video/vesa.cpp',
    'source/cxx/driver/video/vt.cpp',
    'source/cxx/flanterm/flanterm.cpp',
    'source/cxx/flanterm/backends/fb.cpp',
	'source/cxx/driver/ahci.cpp',
	'source/cxx/driver/part.cpp',
    'source/cxx/driver/keyboard.cpp',
    'source/cxx/driver/net/e1000.cpp',
    'source/cxx/util/elf.cpp',
    'source/cxx/util/string.cpp'
)
main_includes = include_directories('include')
nasm_sources = files(
    'source/asm/boot.asm',
    'source/asm/irq.asm',
    'source/asm/syscall.asm'
)

nasm = find_program('nasm')
nasm_gen = generator(nasm, output: '@BASENAME@.o', arguments: ['-f', 'elf64', '-g', '-F', 'dwarf', '@INPUT@', '-o', '@OUTPUT@'])
nasm_objects = nasm_gen.process(nasm_sources)

source_dir = join_paths(meson.project_source_root(), 'source')

ld_script = join_paths(source_dir, 'misc', 'linker.ld')

flags_common = [
	'-I$(INCLUDE)',
	'-ggdb',
	'-O3',
	'-ffreestanding',
	'-fdiagnostics-color=always',
	'-fno-stack-protector',
	'-fno-omit-frame-pointer',
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

flags_c = [
    '-std=c11'
]

flags_cpp = [
    '-std=gnu++2b',
	'-fno-rtti',
	'-fno-exceptions',
	'-fno-use-cxa-atexit',

]

flags_ld = [
	'-T', ld_script,
    '-nostdlib',
	'-Wl,-z,max-page-size=0x1000',
	'-no-pie'
]

lai_proj = subproject('lai')

lai_sources = lai_proj.get_variable('sources')
lai_includes = lai_proj.get_variable('includes')

make_elf = executable(
    'hades.elf', main_sources, lai_sources,
    nasm_objects, 
    include_directories: [main_includes, lai_includes],
    link_depends: ld_script, install: true, c_args: flags_common + flags_c, cpp_args: flags_common + flags_cpp, link_args: flags_ld, build_by_default: false)

script_path = join_paths(meson.project_source_root(), 'scripts', 'post.py')
custom_target(
	'hades',
	depends: make_elf,
	input: make_elf,
	output: '_',
	command: [script_path,
		meson.project_name(), 
		'-k', make_elf,
		
		'-s', join_paths(meson.project_source_root(), 'source'),
        '-r', join_paths(meson.project_source_root(), 'sys'),

		'-b', meson.project_build_root(),
	],
	build_by_default: true
)