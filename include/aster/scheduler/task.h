#ifndef ASTER_SCHEDULER_TASK_H
#define ASTER_SCHEDULER_TASK_H

#include <stdint.h>

#define TASK_NAME_MAX 32

typedef enum {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_FINISHED,
} task_state_t;

typedef void (*task_entry_t)(void *context);

typedef struct {
    uint32_t PID;
    char name[TASK_NAME_MAX];
    task_state_t state;
    task_entry_t entry;
    void *context;
} Task;

#endif
