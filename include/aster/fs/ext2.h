#ifndef ASTER_FS_EXT2_H
#define ASTER_FS_EXT2_H

#include <stdbool.h>

#include "aster/fs/vfs.h"
#include "aster/drivers/ata.h"

bool ext2_init(ata_drive_t drive);
const vfs_ops_t *ext2_get_ops(void);

#endif