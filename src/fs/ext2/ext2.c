#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aster/fs/ext2.h"

#include "aster/debug/logging.h"
#include "aster/string.h"

#define EXT2_SUPERBLOCK_OFFSET 1024ULL
#define EXT2_ROOT_INODE 2U
#define EXT2_SUPER_MAGIC 0xEF53

#define EXT2_S_IFDIR 0x4000
#define EXT2_S_IFREG 0x8000

#define EXT2_N_BLOCKS 15
#define EXT2_DIRECT_BLOCKS 12

typedef struct __attribute__((packed)) {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count_lo;
    uint32_t s_r_blocks_count_lo;
    uint32_t s_free_blocks_count_lo;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t s_uuid[16];
    char s_volume_name[16];
    char s_last_mounted[64];
    uint32_t s_algorithm_usage_bitmap;
} ext2_superblock_t;

typedef struct __attribute__((packed)) {
    uint32_t bg_block_bitmap_lo;
    uint32_t bg_inode_bitmap_lo;
    uint32_t bg_inode_table_lo;
    uint16_t bg_free_blocks_count_lo;
    uint16_t bg_free_inodes_count_lo;
    uint16_t bg_used_dirs_count_lo;
    uint16_t bg_pad;
    uint8_t bg_reserved[12];
} ext2_group_desc_t;

typedef struct __attribute__((packed)) {
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size_lo;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks_lo;
    uint32_t i_flags;
    uint32_t i_osd1;
    uint32_t i_block[EXT2_N_BLOCKS];
    uint32_t i_generation;
    uint32_t i_file_acl_lo;
    uint32_t i_size_high;
    uint32_t i_obso_faddr;
    uint8_t i_osd2[12];
} ext2_inode_t;

typedef struct __attribute__((packed)) {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t name_len;
    uint8_t file_type;
    char name[];
} ext2_dir_entry_t;

typedef struct {
    bool initialized;
    ata_drive_t drive;
    ext2_superblock_t superblock;
    uint32_t block_size;
    uint32_t inode_size;
    uint32_t inodes_per_group;
    uint32_t blocks_per_group;
    uint32_t groups_count;
    uint32_t first_data_block;
} ext2_fs_t;

static ext2_fs_t fs;

static size_t ext2_min_size(size_t a, size_t b) {
    return a < b ? a : b;
}

static bool ext2_read_bytes(uint64_t offset, void *buffer, size_t size) {
    uint8_t sector[ATA_SECTOR_SIZE];
    uint8_t *out = (uint8_t *)buffer;
    size_t copied = 0;

    while (copied < size) {
        uint64_t absolute = offset + copied;
        uint32_t lba = (uint32_t)(absolute / ATA_SECTOR_SIZE);
        size_t sector_offset = (size_t)(absolute % ATA_SECTOR_SIZE);

        if (!ata_read_sector(fs.drive, lba, sector)) {
            return false;
        }

        size_t chunk = ext2_min_size(ATA_SECTOR_SIZE - sector_offset, size - copied);
        memcpy(out + copied, sector + sector_offset, chunk);
        copied += chunk;
    }

    return true;
}

static bool ext2_read_block(uint32_t block_no, void *buffer) {
    return ext2_read_bytes((uint64_t)block_no * fs.block_size, buffer, fs.block_size);
}

static uint64_t ext2_group_desc_offset(void) {
    return fs.block_size == 1024 ? 2048ULL : (uint64_t)fs.block_size;
}

static bool ext2_read_group_desc(uint32_t group, ext2_group_desc_t *out) {
    return ext2_read_bytes(
        ext2_group_desc_offset() + (uint64_t)group * sizeof(ext2_group_desc_t),
        out,
        sizeof(*out)
    );
}

static bool ext2_read_inode(uint32_t inode_no, ext2_inode_t *out) {
    if (inode_no == 0 || out == NULL) {
        return false;
    }

    uint32_t group = (inode_no - 1) / fs.inodes_per_group;
    uint32_t index = (inode_no - 1) % fs.inodes_per_group;
    ext2_group_desc_t desc;

    if (group >= fs.groups_count) {
        return false;
    }

    if (!ext2_read_group_desc(group, &desc)) {
        return false;
    }

    uint64_t inode_offset = (uint64_t)desc.bg_inode_table_lo * fs.block_size + (uint64_t)index * fs.inode_size;
    memset(out, 0, sizeof(*out));

    return ext2_read_bytes(inode_offset, out, ext2_min_size(sizeof(*out), fs.inode_size));
}

static bool ext2_read_block_u32(uint32_t block_no, uint32_t *values, size_t count) {
    uint8_t block[4096];

    if (fs.block_size > sizeof(block)) {
        return false;
    }

    if (!ext2_read_block(block_no, block)) {
        return false;
    }

    size_t limit = ext2_min_size(count * sizeof(uint32_t), fs.block_size);
    memcpy(values, block, limit);
    return true;
}

static bool ext2_inode_block_ref(const ext2_inode_t *inode, uint32_t index, uint32_t *block_no) {
    if (inode == NULL || block_no == NULL) {
        return false;
    }

    if (index < EXT2_DIRECT_BLOCKS) {
        *block_no = inode->i_block[index];
        return *block_no != 0;
    }

    index -= EXT2_DIRECT_BLOCKS;

    if (inode->i_block[12] == 0) {
        return false;
    }

    uint32_t entries_per_block = fs.block_size / sizeof(uint32_t);
    if (index >= entries_per_block) {
        /* Handle double-indirect blocks */
        index -= entries_per_block;

        if (inode->i_block[13] == 0) {
            return false;
        }

        /* Ensure we won't overflow our static buffer sizes */
        if (entries_per_block > 1024) {
            return false;
        }

        uint32_t first_level[1024];
        if (!ext2_read_block_u32(inode->i_block[13], first_level, entries_per_block)) {
            return false;
        }

        uint32_t first_index = index / entries_per_block;
        uint32_t second_index = index % entries_per_block;

        if (first_index >= entries_per_block) {
            return false;
        }

        uint32_t second_block = first_level[first_index];
        if (second_block == 0) {
            return false;
        }

        uint32_t second_level[1024];
        if (!ext2_read_block_u32(second_block, second_level, entries_per_block)) {
            return false;
        }

        *block_no = second_level[second_index];
        return *block_no != 0;
    }

    uint32_t block_map[1024];
    if (entries_per_block > 1024) {
        return false;
    }

    if (!ext2_read_block_u32(inode->i_block[12], block_map, entries_per_block)) {
        return false;
    }

    *block_no = block_map[index];
    return *block_no != 0;
}

static bool ext2_dir_entry_matches(const ext2_dir_entry_t *entry, const char *name, size_t name_len) {
    if (entry->inode == 0 || entry->name_len != name_len) {
        return false;
    }

    return memcmp(entry->name, name, name_len) == 0;
}

static uint32_t ext2_find_in_directory(uint32_t dir_inode_no, const char *name, size_t name_len) {
    ext2_inode_t dir_inode;

    if (!ext2_read_inode(dir_inode_no, &dir_inode)) {
        return 0;
    }

    if ((dir_inode.i_mode & 0xF000) != EXT2_S_IFDIR) {
        return 0;
    }

    uint32_t total_blocks = (dir_inode.i_size_lo + fs.block_size - 1) / fs.block_size;
    uint8_t block[4096];

    if (fs.block_size > sizeof(block)) {
        return 0;
    }

    for (uint32_t block_index = 0; block_index < total_blocks; block_index++) {
        uint32_t data_block = 0;

        if (!ext2_inode_block_ref(&dir_inode, block_index, &data_block)) {
            continue;
        }

        if (!ext2_read_block(data_block, block)) {
            return 0;
        }

        size_t offset = 0;
        while (offset + 8 <= fs.block_size) {
            ext2_dir_entry_t *entry = (ext2_dir_entry_t *)(block + offset);

            if (entry->rec_len < 8 || offset + entry->rec_len > fs.block_size) {
                break;
            }

            if (ext2_dir_entry_matches(entry, name, name_len)) {
                return entry->inode;
            }

            offset += entry->rec_len;
        }
    }

    return 0;
}

static uint32_t ext2_resolve_path(const char *path) {
    if (!fs.initialized || path == NULL) {
        return 0;
    }

    const char *p = path;
    while (*p == '/') {
        p++;
    }

    if (*p == '\0') {
        return EXT2_ROOT_INODE;
    }

    uint32_t current = EXT2_ROOT_INODE;

    while (*p != '\0') {
        size_t component_len = 0;
        while (p[component_len] != '\0' && p[component_len] != '/') {
            component_len++;
        }

        current = ext2_find_in_directory(current, p, component_len);
        if (current == 0) {
            return 0;
        }

        p += component_len;
        while (*p == '/') {
            p++;
        }
    }

    return current;
}

static const char *ext2_lookup_name(const char *path) {
    static char name[256];

    if (path == NULL) {
        return NULL;
    }

    const char *last = path;
    for (const char *p = path; *p != '\0'; p++) {
        if (*p == '/' && p[1] != '\0') {
            last = p + 1;
        }
    }

    size_t len = 0;
    while (last[len] != '\0' && last[len] != '/' && len + 1 < sizeof(name)) {
        name[len] = last[len];
        len++;
    }

    name[len] = '\0';
    return name;
}

static bool ext2_list_dir(
    const char *path,
    void (*callback)(const char *name, bool is_dir, size_t size, void *context),
    void *context
) {
    if (!fs.initialized || callback == NULL) {
        return false;
    }

    uint32_t inode_no = ext2_resolve_path(path ? path : "/");
    if (inode_no == 0) {
        return false;
    }

    ext2_inode_t dir_inode;
    if (!ext2_read_inode(inode_no, &dir_inode)) {
        return false;
    }

    if ((dir_inode.i_mode & 0xF000) != EXT2_S_IFDIR) {
        return false;
    }

    uint32_t total_blocks = (dir_inode.i_size_lo + fs.block_size - 1) / fs.block_size;
    uint8_t block[4096];

    if (fs.block_size > sizeof(block)) {
        return false;
    }

    for (uint32_t block_index = 0; block_index < total_blocks; block_index++) {
        uint32_t data_block = 0;
        if (!ext2_inode_block_ref(&dir_inode, block_index, &data_block)) {
            continue;
        }

        if (!ext2_read_block(data_block, block)) {
            return false;
        }

        size_t offset = 0;
        while (offset + 8 <= fs.block_size) {
            ext2_dir_entry_t *entry = (ext2_dir_entry_t *)(block + offset);

            if (entry->rec_len < 8 || offset + entry->rec_len > fs.block_size) {
                break;
            }

            if (entry->inode != 0 && entry->name_len > 0) {
                ext2_inode_t entry_inode;
                bool is_dir = false;
                size_t size = 0;

                if (ext2_read_inode(entry->inode, &entry_inode)) {
                    is_dir = (entry_inode.i_mode & 0xF000) == EXT2_S_IFDIR;
                    size = entry_inode.i_size_lo;
                }

                char name[256];
                size_t copy = entry->name_len < sizeof(name) - 1 ? entry->name_len : sizeof(name) - 1;
                memcpy(name, entry->name, copy);
                name[copy] = '\0';

                callback(name, is_dir, size, context);
            }

            offset += entry->rec_len;
        }
    }

    return true;
}

static bool ext2_mkdir(const char *path) {
    (void)path;
    return false;
}

static bool ext2_write_file(const char *path, const void *data, size_t size) {
    (void)path;
    (void)data;
    (void)size;
    return false;
}

static size_t ext2_read_file(const char *path, void *buffer, size_t buffer_size) {
    if (path == NULL || buffer == NULL || buffer_size == 0) {
        return 0;
    }

    uint32_t inode_no = ext2_resolve_path(path);
    if (inode_no == 0) {
        return 0;
    }

    ext2_inode_t inode;
    if (!ext2_read_inode(inode_no, &inode)) {
        return 0;
    }

    if ((inode.i_mode & 0xF000) != EXT2_S_IFREG) {
        return 0;
    }

    uint64_t file_size = inode.i_size_lo;
    if (file_size > buffer_size) {
        file_size = buffer_size;
    }

    uint8_t block[4096];
    if (fs.block_size > sizeof(block)) {
        return 0;
    }

    uint8_t *out = (uint8_t *)buffer;
    size_t copied = 0;
    uint32_t block_index = 0;

    while (copied < file_size) {
        uint32_t data_block = 0;
        if (!ext2_inode_block_ref(&inode, block_index++, &data_block)) {
            break;
        }

        if (!ext2_read_block(data_block, block)) {
            break;
        }

        size_t chunk = ext2_min_size(fs.block_size, (size_t)(file_size - copied));
        memcpy(out + copied, block, chunk);
        copied += chunk;
    }

    return copied;
}

static vfs_ops_t ops = {
    .mkdir = ext2_mkdir,
    .write_file = ext2_write_file,
    .read_file = ext2_read_file,
    .lookup_name = ext2_lookup_name,
    .list_dir = ext2_list_dir,
};

bool ext2_init(ata_drive_t drive) {
    memset(&fs, 0, sizeof(fs));
    fs.drive = drive;

    if (!ata_get_device(drive)) {
        return false;
    }

    if (!ext2_read_bytes(EXT2_SUPERBLOCK_OFFSET, &fs.superblock, sizeof(fs.superblock))) {
        return false;
    }

    if (fs.superblock.s_magic != EXT2_SUPER_MAGIC) {
        return false;
    }

    fs.block_size = 1024U << fs.superblock.s_log_block_size;
    fs.inode_size = fs.superblock.s_inode_size != 0 ? fs.superblock.s_inode_size : 128U;
    fs.inodes_per_group = fs.superblock.s_inodes_per_group;
    fs.blocks_per_group = fs.superblock.s_blocks_per_group;
    fs.first_data_block = fs.superblock.s_first_data_block;
    fs.groups_count = (fs.superblock.s_blocks_count_lo + fs.blocks_per_group - 1) / fs.blocks_per_group;

    if (fs.block_size < 1024U || fs.block_size > 4096U) {
        log_warn("ext2: unsupported block size");
        return false;
    }

    if (fs.inodes_per_group == 0 || fs.blocks_per_group == 0) {
        return false;
    }

    fs.initialized = true;

    log_okf(
        "ext2 mounted: block_size=%u inode_size=%u groups=%u",
        (unsigned int)fs.block_size,
        (unsigned int)fs.inode_size,
        (unsigned int)fs.groups_count
    );

    return true;
}

const vfs_ops_t *ext2_get_ops(void) {
    if (!fs.initialized) {
        return NULL;
    }

    return &ops;
}