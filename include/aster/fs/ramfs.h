#ifndef ASTER_FS_RAMFS_H
#define ASTER_FS_RAMFS_H

#include "aster/fs/vfs.h"
#include <stdbool.h>
#include <stddef.h>

bool ramfs_init(void);
const vfs_ops_t *ramfs_get_ops(void);

#endif