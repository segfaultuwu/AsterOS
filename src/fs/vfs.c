#include "aster/fs/vfs.h"
#include "aster/debug/logging.h"

static const vfs_ops_t *root_ops = NULL;

bool vfs_init(void) {
    root_ops = NULL;
    return true;
}

bool vfs_register_root(const vfs_ops_t *ops) {
    if (ops == NULL) return false;
    root_ops = ops;
    return true;
}

bool vfs_mkdir(const char *path) {
    if (root_ops == NULL || root_ops->mkdir == NULL) return false;
    return root_ops->mkdir(path);
}

bool vfs_write_file(const char *path, const void *data, size_t size) {
    if (root_ops == NULL || root_ops->write_file == NULL) return false;
    return root_ops->write_file(path, data, size);
}

size_t vfs_read_file(const char *path, void *buffer, size_t buffer_size) {
    if (root_ops == NULL || root_ops->read_file == NULL) return 0;
    return root_ops->read_file(path, buffer, buffer_size);
}

const char *vfs_lookup_name(const char *path) {
    if (root_ops == NULL || root_ops->lookup_name == NULL) return NULL;
    return root_ops->lookup_name(path);
}
