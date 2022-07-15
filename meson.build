project('trajan', 'cpp', version: '0.0.1')
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
	'source/cxx/fs/devfs.cpp',
	'source/cxx/driver/ahci.cpp',
	'source/cxx/driver/part.cpp'
)
main_includes = include_directories('include')
nasm_sources = files(
    'source/asm/boot.asm',
    'source/asm/irq.asm',
    'source/asm/lock.asm'
)

nasm = find_program('nasm')
nasm_gen = generator(nasm, output: '@BASENAME@.o', arguments: ['-f', 'elf64', '-g', '-F', 'dwarf', '@INPUT@', '-o', '@OUTPUT@'])
nasm_objects = nasm_gen.process(nasm_sources)

source_dir = join_paths(meson.source_root(), 'source')
build_dir = meson.build_root()

ld_script = join_paths(source_dir, 'misc', 'linker.ld')

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

# PROJECT_NAME=$1
# IMG_NAME=$2
# KERNEL_FILE=$3

make_elf = executable('trajan.elf', main_sources, nasm_objects, include_directories: main_includes, link_depends: ld_script, install: true, cpp_args: flags_cpp, link_args: flags_ld, build_by_default: false)
make_trajan = custom_target(
	'trajan',
	depends: make_elf,
	input: make_elf,
	output: '_',
	command: ['scripts/exec_scripts.sh', 
		meson.project_name(), 
		make_elf],
	build_by_default: true
)