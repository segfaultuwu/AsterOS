#include <stddef.h>
#include <stdint.h>

#include "aster/scheduler/scheduler.h"
#include "aster/debug/logging.h"

#define SCHEDULER_MAX_TASKS 16

static Task tasks[SCHEDULER_MAX_TASKS];
static size_t task_count;
static size_t next_task_index;
static uint32_t next_pid = 1;
static Task *current_task;

static void scheduler_clear_task(Task *task) {
    task->PID = 0;
    task->name[0] = '\0';
    task->state = TASK_UNUSED;
    task->entry = NULL;
    task->context = NULL;
}

static void scheduler_copy_name(char dst[TASK_NAME_MAX], const char *src) {
    size_t i = 0;

    if (src == NULL) {
        src = "task";
    }

    while (i + 1 < TASK_NAME_MAX && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

void scheduler_init(void) {
    for (size_t i = 0; i < SCHEDULER_MAX_TASKS; i++) {
        scheduler_clear_task(&tasks[i]);
    }

    task_count = 0;
    next_task_index = 0;
    next_pid = 1;
    current_task = NULL;
}

Task *scheduler_create_task(const char *name, task_entry_t entry, void *context) {
    if (entry == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < SCHEDULER_MAX_TASKS; i++) {
        Task *task = &tasks[i];

        if (task->state != TASK_UNUSED) {
            continue;
        }

        task->PID = next_pid++;
        scheduler_copy_name(task->name, name);
        task->state = TASK_READY;
        task->entry = entry;
        task->context = context;
        task_count++;

        return task;
    }

    log_error("scheduler: task table full");
    return NULL;
}

size_t scheduler_task_count(void) {
    return task_count;
}

Task *scheduler_current_task(void) {
    return current_task;
}

const Task *scheduler_get_task(uint32_t pid) {
    if (pid == 0) {
        return NULL;
    }

    for (size_t i = 0; i < SCHEDULER_MAX_TASKS; i++) {
        if (tasks[i].state != TASK_UNUSED && tasks[i].PID == pid) {
            return &tasks[i];
        }
    }

    return NULL;
}

bool scheduler_set_task_state(uint32_t pid, task_state_t state) {
    Task *task = (Task *)scheduler_get_task(pid);

    if (task == NULL) {
        return false;
    }

    task->state = state;
    return true;
}

bool scheduler_run_once(void) {
    if (task_count == 0) {
        return false;
    }

    for (size_t scanned = 0; scanned < SCHEDULER_MAX_TASKS; scanned++) {
        size_t index = (next_task_index + scanned) % SCHEDULER_MAX_TASKS;
        Task *task = &tasks[index];

        if (task->state != TASK_READY || task->entry == NULL) {
            continue;
        }

        next_task_index = (index + 1) % SCHEDULER_MAX_TASKS;
        current_task = task;
        task->state = TASK_RUNNING;

        task->entry(task->context);

        if (task->state == TASK_RUNNING) {
            task->state = TASK_READY;
        }

        current_task = NULL;
        return true;
    }

    return false;
}

void scheduler_run(void) {
    for (;;) {
        if (!scheduler_run_once()) {
            __asm__ volatile ("hlt");
        }
    }
}