#ifndef ASTER_SCHEDULER_SCHEDULER_H
#define ASTER_SCHEDULER_SCHEDULER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aster/scheduler/task.h"

void scheduler_init(void);
Task *scheduler_create_task(const char *name, task_entry_t entry, void *context);
bool scheduler_run_once(void);
void scheduler_run(void);
size_t scheduler_task_count(void);
Task *scheduler_current_task(void);
const Task *scheduler_get_task(uint32_t pid);
bool scheduler_set_task_state(uint32_t pid, task_state_t state);

#endif