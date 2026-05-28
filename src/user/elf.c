#include "aster/user/elf.h"
#include "aster/mm/pmm.h"
#include "aster/mm/vmm.h"
#include "aster/debug/logging.h"

#include <string.h>

#define ALIGN_DOWN(addr, align) ((addr) & ~((align) - 1))
#define ALIGN_UP(addr, align) (((addr) + (align) - 1) & ~((align) - 1))

/* ELF Header (64-bit) */
typedef struct {
    uint32_t e_magic;
    uint8_t e_class;
    uint8_t e_data;
    uint8_t e_version;
    uint8_t e_osabi;
    uint8_t e_abiversion;
    uint8_t e_pad[7];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version_full;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} elf_header_t;

/* Program Header (64-bit) */
typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} elf_program_header_t;

#define PT_LOAD 1

#define PF_X 1
#define PF_W 2
#define PF_R 4

#define USER_STACK_VADDR 0x00007ffffffde000ULL

bool elf_validate(const void *elf_data, uint64_t elf_size) {
    if (!elf_data || elf_size < sizeof(elf_header_t)) {
        return false;
    }

    const elf_header_t *header = (const elf_header_t *)elf_data;

    /* Check magic number */
    if (header->e_magic != ELF_MAGIC) {
        log_warn("ELF: Invalid magic number");
        return false;
    }

    /* Check class (64-bit) */
    if (header->e_class != ELFCLASS64) {
        log_warn("ELF: Not a 64-bit executable");
        return false;
    }

    /* Check data encoding (little-endian) */
    if (header->e_data != ELFDATA2LSB) {
        log_warn("ELF: Not little-endian");
        return false;
    }

    /* Check machine type (x86-64) */
    if (header->e_machine != EM_X86_64) {
        log_warn("ELF: Not x86-64");
        return false;
    }

    /* Check type (executable or shared object) */
    if (header->e_type != ET_EXEC && header->e_type != ET_DYN) {
        log_warn("ELF: Invalid type");
        return false;
    }

    return true;
}

bool elf_load_user(
    const void *elf_data,
    uint64_t elf_size,
    uint64_t pml4,
    elf_load_result_t *out
) {
    (void)pml4;  /* Will be used in full address space implementation */

    if (!out || !elf_validate(elf_data, elf_size)) {
        return false;
    }

    const elf_header_t *header = (const elf_header_t *)elf_data;
    const elf_program_header_t *phdr_base = (const elf_program_header_t *)
        ((uintptr_t)elf_data + header->e_phoff);

    /* Load all LOAD program headers */
    for (uint16_t i = 0; i < header->e_phnum; i++) {
        const elf_program_header_t *phdr = &phdr_base[i];

        if (phdr->p_type != PT_LOAD) {
            continue;
        }

        uint64_t vaddr = phdr->p_vaddr;
        uint64_t memsz = phdr->p_memsz;
        uint64_t filesz = phdr->p_filesz;
        uint64_t offset = phdr->p_offset;
        uint32_t flags = phdr->p_flags;

        log_infof("ELF: Loading segment at vaddr 0x%lx (filesz=%lu, memsz=%lu)",
                  vaddr, filesz, memsz);

        /* Validate offset */
        if (offset + filesz > elf_size) {
            log_warn("ELF: Segment extends beyond file");
            return false;
        }

        uint64_t vaddr_aligned = ALIGN_DOWN(vaddr, PAGE_SIZE);
        uint64_t vaddr_end = ALIGN_UP(vaddr + memsz, PAGE_SIZE);
        uint64_t num_pages = (vaddr_end - vaddr_aligned) / PAGE_SIZE;

        /* Map physical pages to virtual addresses */
        for (uint64_t page_idx = 0; page_idx < num_pages; page_idx++) {
            uint64_t phys = pmm_alloc_page();
            if (!phys) {
                log_warn("ELF: Failed to allocate page");
                return false;
            }

            pmm_zero_page(phys);

            uint64_t vpage = vaddr_aligned + page_idx * PAGE_SIZE;
            uint64_t vmm_flags = PAGE_PRESENT | PAGE_USER;

            if (flags & PF_W) {
                vmm_flags |= PAGE_WRITABLE;
            }
            if (!(flags & PF_X)) {
                vmm_flags |= PAGE_NX;
            }

            if (!vmm_map_page(vpage, phys, vmm_flags)) {
                log_warn("ELF: Failed to map page");
                return false;
            }
        }

        /* Copy file content */
        if (filesz > 0) {
            const uint8_t *src = (const uint8_t *)elf_data + offset;
            uint8_t *dst = (uint8_t *)(vaddr + pmm_get_hhdm_offset());
            memcpy(dst, src, filesz);
        }
    }

    /* Allocate and map user stack */
    uint64_t stack_base = USER_STACK_VADDR;
    uint64_t stack_pages = PAGE_SIZE * 16;  /* 64 KB stack */

    for (uint64_t i = 0; i < stack_pages / PAGE_SIZE; i++) {
        uint64_t phys = pmm_alloc_page();
        if (!phys) {
            log_warn("ELF: Failed to allocate stack page");
            return false;
        }

        pmm_zero_page(phys);

        if (!vmm_map_page(stack_base + i * PAGE_SIZE, phys,
                          PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER | PAGE_NX)) {
            log_warn("ELF: Failed to map stack page");
            return false;
        }
    }

    out->entry = header->e_entry;
    out->user_stack_top = USER_STACK_VADDR + stack_pages;

    log_infof("ELF: Load complete. Entry: 0x%lx, Stack top: 0x%lx",
              out->entry, out->user_stack_top);

    return true;
}
