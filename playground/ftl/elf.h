#ifndef ELF_SYMBOL_FINDER_H
#define ELF_SYMBOL_FINDER_H

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <elf.h>

struct elf_mapped_file {
    void *base;
    size_t size;
    int fd;
};

static int elf_map_file(const char *path, struct elf_mapped_file *out) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;

    // Get file size (stat = statistics) and check for errors
    struct stat st;
    if (fstat(fd, &st) != 0) { close(fd); return -1; }

    // Check if the file is empty
    if (st.st_size == 0) { close(fd); errno = EINVAL; return -1; }

    // Map the file into memory
    void *map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) { close(fd); return -1; }

    out->base = map;
    out->size = (size_t)st.st_size;
    out->fd = fd;
    return 0;
}

static inline void elf_unmap_file(struct elf_mapped_file *mf) {
    if (mf->base && mf->size) munmap(mf->base, mf->size);
    if (mf->fd >= 0) close(mf->fd);
    mf->base = NULL; mf->size = 0; mf->fd = -1;
}

static int elf_find_symbol_in_section(const void *base, size_t size,
                                             const Elf64_Ehdr *elf_header,
                                             const Elf64_Shdr *section_headers,
                                             const Elf64_Shdr *symbol_headers,
                                             const char *needle,
                                             Elf64_Addr *address) {

    // TODO: make validation separate from searching so this function does one thing
    // Make sure the symbol section is valid
    if (symbol_headers->sh_entsize == 0) return -1;
    if (symbol_headers->sh_offset + symbol_headers->sh_size > size) return -1;
    if (symbol_headers->sh_link >= elf_header->e_shnum) return -1;

    // Make sure the string table section is valid
    const Elf64_Shdr *string_table_section = &section_headers[symbol_headers->sh_link];
    if (string_table_section->sh_type != SHT_STRTAB && string_table_section->sh_type != SHT_DYNSYM) return -1;
    if (string_table_section->sh_offset + string_table_section->sh_size > size) return -1;

    const char *strtab = (const char *)base + string_table_section->sh_offset;
    const Elf64_Sym *symbols = (const Elf64_Sym *)((const uint8_t *)base + symbol_headers->sh_offset);

    // Iterate over all symbols in the section, using the string table to find the name
    // and the symbol table to find the address
    size_t num_sumbols = symbol_headers->sh_size / symbol_headers->sh_entsize;
    for (size_t i = 0; i < num_sumbols; ++i) {

        const Elf64_Sym *symbol = &symbols[i];
        if (symbol->st_name >= string_table_section->sh_size) continue;

        const char *haystack = strtab + symbol->st_name;
        if (strcmp(haystack, needle) == 0) {
            *address = symbol->st_value;
            return 0;
        }
    }
    return -1;
}

static int elf_find_symbol(const char *path, const char *symbol,
                                  Elf64_Addr *out_value) {
    // TODO: make mapping separate so this function does only one thing
    const char **out_section_name;
    struct elf_mapped_file mf = {0};
    if (elf_map_file(path, &mf) != 0) return -1;

    const uint8_t *base = (const uint8_t *)mf.base;
    size_t size = mf.size;
    if (size < sizeof(Elf64_Ehdr)) { elf_unmap_file(&mf); return -1; }

    const Elf64_Ehdr *elf_header = (const Elf64_Ehdr *)base;

    // TODO: make validation separate from searching so this function does one thing
    // Make sure the file is a valid ELF64 file
    if (memcmp(elf_header->e_ident, ELFMAG, SELFMAG) != 0) { elf_unmap_file(&mf); return -1; }
    if (elf_header->e_ident[EI_CLASS] != ELFCLASS64) { elf_unmap_file(&mf); return -1; }
    if (elf_header->e_shoff == 0 || elf_header->e_shnum == 0) { elf_unmap_file(&mf); return -1; }
    if (elf_header->e_shoff + (size_t)elf_header->e_shnum * sizeof(Elf64_Shdr) > size) { elf_unmap_file(&mf); return -1; }


    // Search for the symbol in all symbol table sections
    const Elf64_Shdr *shdrs = (const Elf64_Shdr *)(base + elf_header->e_shoff);
    int found = 0;
    for (int i = 0; i < (int)elf_header->e_shnum; ++i) {
        const Elf64_Shdr *sh = &shdrs[i];
        if (sh->sh_type == SHT_SYMTAB || sh->sh_type == SHT_DYNSYM) {
            if (elf_find_symbol_in_section(
                  base, 
                  size, 
                  elf_header, 
                  shdrs, 
                  sh,
                  symbol, 
                  out_value
                ) == 0) {
                found = 1;
                break;
            }
        }
    }

    elf_unmap_file(&mf);
    return found ? 0 : -1;
    // TODO: doesn't work for relocations
}

#endif // ELF_SYMBOL_FINDER_H

