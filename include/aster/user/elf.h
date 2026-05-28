#ifndef ASTER_USER_ELF_H
#define ASTER_USER_ELF_H

#include <stdint.h>
#include <stdbool.h>

/* ELF Header Magic */
#define ELF_MAGIC 0x464c457f

/* ELF File Class */
#define ELFCLASS64 2

/* ELF Data Encoding */
#define ELFDATA2LSB 1

/* ELF Type */
#define ET_EXEC  2
#define ET_DYN   3

/* ELF Machine Type */
#define EM_X86_64 62

typedef struct {
    uint64_t entry;
    uint64_t user_stack_top;
} elf_load_result_t;

/* ELF validation and loading */
bool elf_validate(const void *elf_data, uint64_t elf_size);
bool elf_load_user(
    const void *elf_data,
    uint64_t elf_size,
    uint64_t pml4,
    elf_load_result_t *out
);

#endif
