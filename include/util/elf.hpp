#ifndef ELF_HPP
#define ELF_HPP


#include "fs/vfs.hpp"
#include <frg/vector.hpp>
#include <mm/mm.hpp>
#include <mm/vmm.hpp>
#include <cstddef>
#include <cstdint>

#define ELF_SIGNATURE 0x464C457F
#define ELF_ELF64 0x2

#define ELF_EI_CLASS 0x4
#define ELF_EI_DATA 0x5
#define ELF_EI_VERSION 0x6
#define ELF_EI_OSABI 0x7

#define ELF_EI_SYSTEM_V 0x0
#define ELF_EI_LINUX 0x3

#define ELF_LITTLE_ENDIAN 0x1
#define ELF_MACH_X86_64 0x3e

#define ELF_AT_ENTRY 9
#define ELF_AT_PHDR 3
#define ELF_AT_PHENT 4
#define ELF_AT_PHNUM 5

#define ELF_PT_NULL 0x0
#define ELF_PT_LOAD 0x1
#define ELF_PT_DYNAMIC 0x2
#define ELF_PT_INTERP 0x3
#define ELF_PT_NOTE 0x4
#define ELF_PT_SHLIB 0x5
#define ELF_PT_PHDR 0x6
#define ELF_PT_LTS 0x7
#define ELF_PT_LOOS 0x60000000
#define ELF_PT_HOIS 0x6fffffff
#define ELF_PT_LOPROC 0x70000000
#define ELF_PT_HIPROC 0x7fffffff

#define ELF_PF_X 0x1
#define ELF_PF_W 0x2
#define ELF_PF_R 0x4

#define SHT_SYMTAB 0x2
#define SHT_STRTAB 0x3

#define SHF_ALLOC 0x2

#define STT_FUNC 0x2

namespace elf {
    struct aux {
        uint64_t at_phnum;
        uint64_t at_phent;
        uint64_t at_phdr;
        uint64_t at_entry;
    };

    struct [[gnu::packed]] elf64_hdr {
        uint8_t ident[16];
        uint16_t type;
        uint16_t machine;
        uint32_t version;
        uint64_t entry;
        uint64_t phoff;
        uint64_t shoff;
        uint32_t flags;
        uint16_t hdr_size;
        uint16_t phdr_size;
        uint16_t ph_num;
        uint16_t shdr_size;
        uint16_t sh_num;
        uint16_t shstrndx;
    };

    struct [[gnu::packed]] elf64_phdr {
        uint32_t p_type;
        uint32_t p_flags;
        uint64_t p_offset;
        uint64_t p_vaddr;
        uint64_t p_paddr;
        uint64_t p_filesz;
        uint64_t p_memsz;
        uint64_t p_align;
    };

    struct [[gnu::packed]] elf64_shdr {
        uint32_t sh_name;
        uint32_t sh_type;
        uint64_t sh_flags;
        uint64_t sh_addr;
        uint64_t sh_offset;
        uint64_t sh_size;
        uint32_t sh_link;
        uint32_t sh_info;
        uint64_t sh_addr_align;
        uint64_t sh_entsize;
    };

    struct [[gnu::packed]] elf64_symtab {
        uint32_t st_name;
        unsigned char st_info;
        unsigned char st_other;
        uint16_t st_shndx;
        uint64_t st_value;
        uint64_t st_size;
    };

    struct symbol {
        const char *name;
        uintptr_t addr;
        size_t len;
    };

    struct file {
        uintptr_t load_offset;
        vfs::fd *fd;
        memory::vmm::vmm_ctx *ctx;

        elf64_hdr *header;
        elf::aux aux;

        elf64_phdr *phdrs;
        elf64_shdr *shdrs;
        elf64_shdr *shstrtab_hdrs;
        elf64_shdr *strtab_hdrs;
        elf64_shdr *symtab_hdrs;

        void *shstrtab;
        void *strtab;
        void *symtab;

        frg::vector<symbol, memory::mm::heap_allocator> symbols;

        symbol *find_symbol(uintptr_t addr);

        bool init(vfs::fd *fd);
        void load();
        bool load_interp(char **interp_path);
        void load_aux();
    };

};

#endif