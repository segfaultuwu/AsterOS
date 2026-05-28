// Kernel-side launcher for the embedded user shell blob.

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "aster/user/shell.h"
#include "aster/scheduler/scheduler.h"
#include "aster/mm/pmm.h"
#include "aster/mm/vmm.h"
#include "aster/arch/x86_64/usermode.h"
#include "aster/debug/logging.h"

#define SHELL_USER_BASE  0x0000010000400000ULL
#define SHELL_STACK_BASE 0x0000010000800000ULL
#define SHELL_STACK_PAGES 16ULL

extern const uint8_t _binary_usershell_bin_start[];
extern const uint8_t _binary_usershell_bin_end[];

static void *phys_to_virt(uint64_t phys) {
	return (void *)(pmm_get_hhdm_offset() + phys);
}

static void shell_task_entry(void *context) {
	(void)context;

	const uint8_t *blob_start = _binary_usershell_bin_start;
	const uint8_t *blob_end = _binary_usershell_bin_end;
	size_t blob_size = (size_t)(blob_end - blob_start);
	size_t blob_pages = (blob_size + PAGE_SIZE - 1ULL) / PAGE_SIZE;

	for (size_t i = 0; i < blob_pages; i++) {
		uint64_t phys = pmm_alloc_page();
		if (phys == 0) {
			log_error("shell_spawn: failed to allocate blob page");
			return;
		}

		pmm_zero_page(phys);

		if (!vmm_map_page(
				SHELL_USER_BASE + (uint64_t)i * PAGE_SIZE,
				phys,
				PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER)) {
			log_error("shell_spawn: failed to map blob page");
			return;
		}

		uint8_t *dst = (uint8_t *)phys_to_virt(phys);
		size_t offset = i * PAGE_SIZE;
		size_t chunk = blob_size - offset;
		if (chunk > PAGE_SIZE) {
			chunk = PAGE_SIZE;
		}

		for (size_t j = 0; j < chunk; j++) {
			dst[j] = blob_start[offset + j];
		}
	}

	for (size_t i = 0; i < SHELL_STACK_PAGES; i++) {
		uint64_t phys = pmm_alloc_page();
		if (phys == 0) {
			log_error("shell_spawn: failed to allocate stack page");
			return;
		}

		pmm_zero_page(phys);

		if (!vmm_map_page(
				SHELL_STACK_BASE + (uint64_t)i * PAGE_SIZE,
				phys,
				PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER | PAGE_NX)) {
			log_error("shell_spawn: failed to map stack page");
			return;
		}
	}

	log_ok("shell_spawn: entering user shell");
	usermode_enter(SHELL_USER_BASE, SHELL_STACK_BASE + SHELL_STACK_PAGES * PAGE_SIZE);
}

bool shell_spawn(void) {
	return scheduler_create_task("shell", shell_task_entry, NULL) != NULL;
}
