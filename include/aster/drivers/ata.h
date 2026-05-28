#ifndef ASTER_DRIVERS_ATA_H
#define ASTER_DRIVERS_ATA_H

#include <stdint.h>
#include <stdbool.h>

#define ATA_SECTOR_SIZE 512

typedef enum {
    ATA_PRIMARY_MASTER = 0,
    ATA_PRIMARY_SLAVE,
    ATA_SECONDARY_MASTER,
    ATA_SECONDARY_SLAVE,
} ata_drive_t;

typedef struct {
    bool present;
    ata_drive_t drive;
    uint16_t io_base;
    uint16_t ctrl_base;
    uint8_t slave;
    uint32_t sectors_28;
    char model[41];
} ata_device_t;

void ata_init(void);

const ata_device_t *ata_get_device(ata_drive_t drive);

bool ata_read_sector(ata_drive_t drive, uint32_t lba, void *buffer);
bool ata_read_sectors(ata_drive_t drive, uint32_t lba, uint8_t count, void *buffer);

void ata_init(void);

void ata_test_write(void);

bool ata_write_sector(ata_drive_t drive, uint32_t lba, const void *buffer);
bool ata_write_sectors(ata_drive_t drive, uint32_t lba, uint8_t count, const void *buffer);
#endif
