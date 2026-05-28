#ifndef ASTER_FS_VFS_H
#define ASTER_FS_VFS_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    bool (*mkdir)(const char *path);
    bool (*write_file)(const char *path, const void *data, size_t size);
    size_t (*read_file)(const char *path, void *buffer, size_t buffer_size);
    const char *(*lookup_name)(const char *path);
    bool (*list_dir)(const char *path, void (*callback)(const char *name, bool is_dir, size_t size, void *context), void *context);
} vfs_ops_t;

bool vfs_init(void);
bool vfs_mount_root(const vfs_ops_t *ops);
bool vfs_register_root(const vfs_ops_t *ops);
bool vfs_mkdir(const char *path);
bool vfs_write_file(const char *path, const void *data, size_t size);
size_t vfs_read_file(const char *path, void *buffer, size_t buffer_size);
const char *vfs_lookup_name(const char *path);
bool vfs_list_dir(const char *path, void (*callback)(const char *name, bool is_dir, size_t size, void *context), void *context);

#endif