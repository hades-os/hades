#include "fs/vfs.hpp"
#include "mm/common.hpp"
#include "mm/mm.hpp"
#include "mm/pmm.hpp"
#include "mm/vmm.hpp"
#include "util/log/log.hpp"
#include "util/string.hpp"
#include <cstddef>
#include <cstdint>
#include <util/elf.hpp>

bool check_hdr(elf::elf64_hdr *header) {
    uint32_t sig = *(uint32_t *) header;
    if (sig != ELF_SIGNATURE) {
        return false;
    }

    if (header->ident[ELF_EI_OSABI] != ELF_EI_SYSTEM_V && header->ident[ELF_EI_OSABI] != ELF_EI_LINUX) return false;
    if (header->ident[ELF_EI_DATA] != ELF_LITTLE_ENDIAN) return false;
    if (header->ident[ELF_EI_CLASS] != ELF_ELF64) return false;
    if (header->machine != ELF_MACH_X86_64 && header->machine != 0) return false;

    return true;
}

const char *extract_string(void *data, uint32_t index) {
    return (const char *)((char *) data + index);
}

elf::elf64_shdr *find_section(elf::file *file, uint32_t type, const char *name) {
    elf::elf64_shdr *shdr = file->shdr;

    for (size_t i = 0; i < file->header->sh_num; i++) {
        if (shdr[i].sh_type != type) {
            continue;
        }

        const char *sec_name = extract_string(file->shstrtab, shdr[i].sh_name);
        if (strcmp(sec_name, name) == 0) {
            return &shdr[i];
        }
    }

    return nullptr;
}

bool init_symbols(elf::file *file) {
    if (file->symtab_hdr->sh_entsize != sizeof(elf::elf64_symtab)) {
        return false;
    }

    uint64_t ents = file->symtab_hdr->sh_size / file->symtab_hdr->sh_entsize;
    elf::elf64_symtab *symtab = (elf::elf64_symtab *) file->symtab;

    for (size_t i = 0; i < ents; i++) {
        if (!(symtab[i].st_info & STT_FUNC)) {
            continue;
        }

        auto symbol = elf::symbol{
            .name = extract_string(file->strtab, symtab[i].st_name),
            .addr = symtab[i].st_value,
            .len = symtab[i].st_size
        };

        file->symbols.push_back(symbol);
    }

    return true;
}

bool elf::file::init(vfs::fd *fd) {
    this->fd = fd;

    elf64_hdr *hdr = (elf64_hdr *) kmalloc(1024);
    auto res = vfs::read(fd, hdr, 1024);
    if (res != 1024) {
        return false;
    }

    res = check_hdr(hdr);
    if (!res) {
        return false;
    }

    this->header = hdr;
    this->phdr = (elf64_phdr *) kcalloc(hdr->ph_num, sizeof(elf64_phdr));
    this->shdr = (elf64_shdr *) kcalloc(hdr->sh_num, sizeof(elf64_shdr));

    vfs::lseek(fd, hdr->shoff, vfs::sflags::SET);
    res = vfs::read(fd, shdr, hdr->sh_num * sizeof(elf64_shdr));
    if (res < 0) {
        return false;
    }

    vfs::lseek(fd, hdr->phoff, vfs::sflags::SET);
    res = vfs::read(fd, phdr, hdr->ph_num * sizeof(elf64_phdr));
    if (res < 0) {
        return false;
    }

    shstrtab_hdr = shdr + header->shstrndx;
    shstrtab = memory::pmm::alloc((shstrtab_hdr->sh_size / memory::common::page_size) + 1);
    
    vfs::lseek(fd, shstrtab_hdr->sh_offset, vfs::sflags::SET);
    res = vfs::read(fd, shstrtab, shstrtab_hdr->sh_size);

    if (res != shstrtab_hdr->sh_size) {
        return false;
    }

    strtab_hdr = find_section(this, SHT_STRTAB, ".strtab");
    if (strtab_hdr == nullptr) {
        return false;
    }

    strtab = memory::pmm::alloc((strtab_hdr->sh_size / memory::common::page_size) + 1);
    
    vfs::lseek(fd, strtab_hdr->sh_offset, vfs::sflags::SET);
    res = vfs::read(fd, strtab, strtab_hdr->sh_size);

    if (res != strtab_hdr->sh_size) {
        return false;
    }

    symtab_hdr = find_section(this, SHT_SYMTAB, ".symtab");
    if (symtab_hdr == nullptr) {
        return false;
    }

    symtab = memory::pmm::alloc((symtab_hdr->sh_size / memory::common::page_size) + 1);

    vfs::lseek(fd, symtab_hdr->sh_offset, vfs::sflags::SET);
    res = vfs::read(fd, symtab, symtab_hdr->sh_size);
    if (res != symtab_hdr->sh_size) {
        return false;
    }

    res = init_symbols(this);
    if (!res) {
        return false;
    }

    return true;
}

void elf::file::load() {
    for (size_t i = 0; i < header->ph_num; i++) {
        if (phdr[i].p_type != ELF_PT_LOAD) {
            continue;
        }

        elf64_phdr *phdr = &this->phdr[i];
        size_t misalign = phdr->p_vaddr & (memory::common::page_size - 1);
        size_t pages = ((misalign + phdr->p_memsz) / memory::common::page_size) + 1;

        if ((misalign + phdr->p_memsz) > memory::common::page_size) {
            pages = pages + 1;
        }

        memory::vmm::map((void *)(phdr->p_vaddr + load_offset - misalign), pages * memory::common::page_size, VMM_PRESENT | VMM_WRITE | VMM_USER | VMM_FIXED | VMM_MANAGED, ctx);

        vfs::lseek(fd, phdr->p_offset, vfs::sflags::SET);
        vfs::read(fd, (void *)(phdr->p_vaddr + load_offset), phdr->p_filesz);
    }
}

bool elf::file::load_interp(char **interp_path) {
    elf64_phdr *interp_hdr = nullptr;

    for (size_t i = 0; i < header->ph_num; i++) {
        if (phdr[i].p_type == ELF_PT_INTERP) {
            interp_hdr = &phdr[i];
        }
    }

    if (interp_hdr == nullptr) {
        return false;
    }

    *interp_path = (char *) kmalloc(phdr->p_filesz + 1);

    vfs::lseek(fd, phdr->p_offset, vfs::sflags::SET);
    vfs::read(fd, *interp_path, phdr->p_filesz);

    return true;
}

void elf::file::load_aux() {
    aux.at_phdr = 0;
    aux.at_phent = sizeof(elf64_phdr);
    aux.at_phnum = header->ph_num;
    aux.at_entry = load_offset + header->entry;

    for (size_t i = 0; i < header->ph_num; i++) {
        if (phdr[i].p_type == ELF_PT_PHDR) {
            aux.at_phdr = load_offset + phdr[i].p_vaddr;
        }
    }
}