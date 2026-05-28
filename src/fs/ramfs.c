#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "aster/fs/ramfs.h"
#include "aster/fs/vfs.h"
#include "aster/debug/logging.h"
#define RAMFS_MAX_NODES 128
#define RAMFS_NAME_MAX 32
#define RAMFS_FILE_CAPACITY 2048

typedef enum { RAMFS_UNUSED=0, RAMFS_DIR, RAMFS_FILE } ramfs_type_t;

typedef struct ramfs_node {
    ramfs_type_t type;
    uint32_t parent;
    uint32_t first_child;
    uint32_t next_sibling;
    char name[RAMFS_NAME_MAX];
    uint8_t data[RAMFS_FILE_CAPACITY];
    size_t size;
} ramfs_node_t;

static ramfs_node_t nodes[RAMFS_MAX_NODES];
static bool initialized = false;

static uint8_t lower(uint8_t c) { if (c >= 'A' && c <= 'Z') return c - 'A' + 'a'; return c; }

static bool names_equal_ci(const char *a, const char *b) {
    while (*a && *b) {
        if (lower((uint8_t)*a) != lower((uint8_t)*b)) return false;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

static void clear_node(ramfs_node_t *n) {
    n->type = RAMFS_UNUSED;
    n->parent = 0;
    n->first_child = 0;
    n->next_sibling = 0;
    n->name[0] = '\0';
    n->size = 0;
}

static void set_name(char *dst, const char *src) {
    size_t i = 0;
    while (i + 1 < RAMFS_NAME_MAX && src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static uint32_t alloc_node(void) {
    for (uint32_t i = 1; i < RAMFS_MAX_NODES; i++) {
        if (nodes[i].type == RAMFS_UNUSED) {
            clear_node(&nodes[i]);
            return i;
        }
    }
    return 0;
}

static const char *skip_slash(const char *p) { while (*p == '/') p++; return p; }

static size_t comp_len(const char *p) { size_t l=0; while (p[l] && p[l] != '/') l++; return l; }

static uint32_t find_child(uint32_t parent, const char *name) {
    uint32_t c = nodes[parent].first_child;
    while (c) {
        if (names_equal_ci(nodes[c].name, name)) {
            return c;
        }
        c = nodes[c].next_sibling;
    }
    return 0;
}

static ramfs_node_t *lookup_node(const char *path) {
    const char *p = skip_slash(path);
    uint32_t cur = 0;
    if (!*p) return &nodes[0];
    while (*p) {
        size_t len = comp_len(p);
        char comp[RAMFS_NAME_MAX];
        size_t copy = len < (RAMFS_NAME_MAX-1) ? len : (RAMFS_NAME_MAX-1);
        for (size_t i=0;i<copy;i++) {
            comp[i]=p[i];
        }
        comp[copy]='\0';
        uint32_t child = find_child(cur, comp);
        if (!child) return NULL;
        cur = child;
        p = skip_slash(p+len);
    }
    return &nodes[cur];
}

static bool ramfs_mkdir(const char *path) {
    if (!initialized) return false;
    const char *p = skip_slash(path);
    uint32_t cur = 0;
    if (!*p) return true;
    while (*p) {
        size_t len = comp_len(p);
        char comp[RAMFS_NAME_MAX];
        size_t copy = len < (RAMFS_NAME_MAX-1) ? len : (RAMFS_NAME_MAX-1);
        for (size_t i=0;i<copy;i++) {
            comp[i]=p[i];
        }
        comp[copy]='\0';
        uint32_t child = find_child(cur, comp);
        if (!child) {
            uint32_t node = alloc_node();
            if (!node) return false;
            nodes[node].type = RAMFS_DIR;
            nodes[node].parent = cur;
            set_name(nodes[node].name, comp);
            nodes[node].next_sibling = nodes[cur].first_child;
            nodes[cur].first_child = node;
            child = node;
        }
        cur = child;
        p = skip_slash(p+len);
    }
    return true;
}

static bool ramfs_write_file(const char *path, const void *data, size_t size) {
    if (!initialized) return false;
    const char *p = skip_slash(path);
    uint32_t cur = 0;
    while (*p) {
        size_t len = comp_len(p);
        char comp[RAMFS_NAME_MAX];
        size_t copy = len < (RAMFS_NAME_MAX-1) ? len : (RAMFS_NAME_MAX-1);
        for (size_t i=0;i<copy;i++) {
            comp[i]=p[i];
        }
        comp[copy]='\0';
        const char *next = skip_slash(p+len);
        if (!*next) {
            // last component = file name
            uint32_t file = find_child(cur, comp);
            if (!file) {
                file = alloc_node();
                if (!file) return false;
                nodes[file].type = RAMFS_FILE;
                nodes[file].parent = cur;
                set_name(nodes[file].name, comp);
                nodes[file].next_sibling = nodes[cur].first_child;
                nodes[cur].first_child = file;
            } else if (nodes[file].type != RAMFS_FILE) {
                return false;
            }
            if (size > RAMFS_FILE_CAPACITY) return false;
            memcpy(nodes[file].data, data, size);
            nodes[file].size = size;
            return true;
        }
        uint32_t child = find_child(cur, comp);
        if (!child) {
            // create intermediate dir
            child = alloc_node();
            if (!child) return false;
            nodes[child].type = RAMFS_DIR;
            nodes[child].parent = cur;
            set_name(nodes[child].name, comp);
            nodes[child].next_sibling = nodes[cur].first_child;
            nodes[cur].first_child = child;
        } else if (nodes[child].type != RAMFS_DIR) {
            return false;
        }
        cur = child;
        p = next;
    }
    return false;
}

static size_t ramfs_read_file(const char *path, void *buffer, size_t buffer_size) {
    if (!initialized) return 0;
    ramfs_node_t *n = lookup_node(path);
    if (!n || n->type != RAMFS_FILE) return 0;
    size_t want = n->size < buffer_size ? n->size : buffer_size;
    memcpy(buffer, n->data, want);
    return want;
}

static const char *ramfs_lookup_name(const char *path) {
    ramfs_node_t *n = lookup_node(path);
    if (!n) return NULL;
    return n->name;
}

static vfs_ops_t ops = {
    .mkdir = ramfs_mkdir,
    .write_file = ramfs_write_file,
    .read_file = ramfs_read_file,
    .lookup_name = ramfs_lookup_name,
};

bool ramfs_init(void) {
    if (initialized) return true;
    for (uint32_t i=0;i<RAMFS_MAX_NODES;i++) clear_node(&nodes[i]);
    nodes[0].type = RAMFS_DIR;
    set_name(nodes[0].name, "/");
    initialized = true;
    return true;
}

const vfs_ops_t *ramfs_get_ops(void) { return &ops; }
