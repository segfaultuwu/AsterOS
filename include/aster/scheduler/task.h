#ifndef ASTER_SCHEDULER_TASK_H
#define ASTER_SCHEDULER_TASK_H

#include <stdint.h>

typedef struct {
    uint32_t PID;
    char* name;
} Task;

#endif
