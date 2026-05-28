#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "aster/drivers/ata.h"
#include "aster/drivers/ports.h"
#include "aster/debug/logging.h"

#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CTRL    0x3F6
#define ATA_SECONDARY_IO    0x170
#define ATA_SECONDARY_CTRL  0x376

#define ATA_REG_DATA        0
#define ATA_REG_ERROR       1
#define ATA_REG_FEATURES    1
#define ATA_REG_SECCOUNT0   2
#define ATA_REG_LBA0        3
#define ATA_REG_LBA1        4
#define ATA_REG_LBA2        5
#define ATA_REG_HDDEVSEL    6
#define ATA_REG_COMMAND     7
#define ATA_REG_STATUS      7

#define ATA_REG_ALTSTATUS   0
#define ATA_REG_CONTROL     0

#define ATA_CMD_IDENTIFY    0xEC
#define ATA_CMD_READ_PIO    0x20

#define ATA_SR_BSY          0x80
#define ATA_SR_DRDY         0x40
#define ATA_SR_DF           0x20
#define ATA_SR_DRQ          0x08
#define ATA_SR_ERR          0x01

#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_CACHE_FLUSH 0xE7

static ata_device_t ata_devices[4];

static void ata_delay_400ns(uint16_t ctrl_base) {
    inb(ctrl_base + ATA_REG_ALTSTATUS);
    inb(ctrl_base + ATA_REG_ALTSTATUS);
    inb(ctrl_base + ATA_REG_ALTSTATUS);
    inb(ctrl_base + ATA_REG_ALTSTATUS);
}

static uint16_t ata_io_base(ata_drive_t drive) {
    if (drive == ATA_PRIMARY_MASTER || drive == ATA_PRIMARY_SLAVE) {
        return ATA_PRIMARY_IO;
    }

    return ATA_SECONDARY_IO;
}

static uint16_t ata_ctrl_base(ata_drive_t drive) {
    if (drive == ATA_PRIMARY_MASTER || drive == ATA_PRIMARY_SLAVE) {
        return ATA_PRIMARY_CTRL;
    }

    return ATA_SECONDARY_CTRL;
}

static uint8_t ata_slave_bit(ata_drive_t drive) {
    return (drive == ATA_PRIMARY_SLAVE || drive == ATA_SECONDARY_SLAVE) ? 1 : 0;
}

static uint8_t ata_status(uint16_t io_base) {
    return inb(io_base + ATA_REG_STATUS);
}

static bool ata_wait_not_busy(uint16_t io_base) {
    for (uint32_t i = 0; i < 1000000; i++) {
        uint8_t status = ata_status(io_base);

        if ((status & ATA_SR_BSY) == 0) {
            return true;
        }
    }

    return false;
}

static bool ata_wait_drq(uint16_t io_base) {
    for (uint32_t i = 0; i < 1000000; i++) {
        uint8_t status = ata_status(io_base);

        if (status & ATA_SR_ERR) {
            return false;
        }

        if (status & ATA_SR_DF) {
            return false;
        }

        if ((status & ATA_SR_BSY) == 0 && (status & ATA_SR_DRQ)) {
            return true;
        }
    }

    return false;
}

static void ata_read_words(uint16_t io_base, uint16_t *buffer, uint32_t words) {
    for (uint32_t i = 0; i < words; i++) {
        buffer[i] = inw(io_base + ATA_REG_DATA);
    }
}

static void ata_string_from_identify(char *dst, const uint16_t *identify, uint32_t start_word, uint32_t word_count) {
    uint32_t out = 0;

    for (uint32_t i = 0; i < word_count; i++) {
        uint16_t word = identify[start_word + i];

        dst[out++] = (char)((word >> 8) & 0xFF);
        dst[out++] = (char)(word & 0xFF);
    }

    dst[out] = '\0';

    while (out > 0 && dst[out - 1] == ' ') {
        dst[out - 1] = '\0';
        out--;
    }
}

static bool ata_identify(ata_drive_t drive) {
    uint16_t io_base = ata_io_base(drive);
    uint16_t ctrl_base = ata_ctrl_base(drive);
    uint8_t slave = ata_slave_bit(drive);

    ata_device_t *dev = &ata_devices[drive];

    dev->present = false;
    dev->drive = drive;
    dev->io_base = io_base;
    dev->ctrl_base = ctrl_base;
    dev->slave = slave;
    dev->sectors_28 = 0;
    dev->model[0] = '\0';

    outb(ctrl_base + ATA_REG_CONTROL, 0x02); // disable IRQs for now

    outb(io_base + ATA_REG_HDDEVSEL, (uint8_t)(0xA0 | (slave << 4)));
    ata_delay_400ns(ctrl_base);

    outb(io_base + ATA_REG_SECCOUNT0, 0);
    outb(io_base + ATA_REG_LBA0, 0);
    outb(io_base + ATA_REG_LBA1, 0);
    outb(io_base + ATA_REG_LBA2, 0);
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    uint8_t status = ata_status(io_base);

    if (status == 0) {
        return false;
    }

    if (!ata_wait_not_busy(io_base)) {
        return false;
    }

    uint8_t lba1 = inb(io_base + ATA_REG_LBA1);
    uint8_t lba2 = inb(io_base + ATA_REG_LBA2);

    if (lba1 != 0 || lba2 != 0) {
        // Probably ATAPI or non-ATA device.
        return false;
    }

    if (!ata_wait_drq(io_base)) {
        return false;
    }

    uint16_t identify[256];
    ata_read_words(io_base, identify, 256);

    dev->present = true;

    // Words 60-61: total 28-bit LBA sectors.
    dev->sectors_28 = ((uint32_t)identify[61] << 16) | identify[60];

    // Words 27-46: model string, 40 bytes.
    ata_string_from_identify(dev->model, identify, 27, 20);

    log_okf(
        "ATA drive found: drive=%u io=0x%x sectors=%u model=%s",
        (unsigned int)drive,
        (unsigned int)io_base,
        (unsigned int)dev->sectors_28,
        dev->model
    );

    return true;
}

void ata_init(void) {
    log_info("Initializing ATA PIO");

    for (uint32_t i = 0; i < 4; i++) {
        ata_identify((ata_drive_t)i);
    }
}

const ata_device_t *ata_get_device(ata_drive_t drive) {
    if ((uint32_t)drive >= 4) {
        return NULL;
    }

    return &ata_devices[drive];
}

bool ata_read_sector(ata_drive_t drive, uint32_t lba, void *buffer) {
    return ata_read_sectors(drive, lba, 1, buffer);
}

bool ata_read_sectors(ata_drive_t drive, uint32_t lba, uint8_t count, void *buffer) {
    if ((uint32_t)drive >= 4 || buffer == NULL || count == 0) {
        return false;
    }

    ata_device_t *dev = &ata_devices[drive];

    if (!dev->present) {
        log_error("ATA read: device not present");
        return false;
    }

    if (lba + count > dev->sectors_28) {
        log_error("ATA read: LBA out of range");
        return false;
    }

    uint16_t io_base = dev->io_base;
    uint16_t ctrl_base = dev->ctrl_base;

    if (!ata_wait_not_busy(io_base)) {
        log_error("ATA read: busy timeout");
        return false;
    }

    outb(io_base + ATA_REG_HDDEVSEL, (uint8_t)(0xE0 | (dev->slave << 4) | ((lba >> 24) & 0x0F)));
    ata_delay_400ns(ctrl_base);

    outb(io_base + ATA_REG_SECCOUNT0, count);
    outb(io_base + ATA_REG_LBA0, (uint8_t)(lba & 0xFF));
    outb(io_base + ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFF));
    outb(io_base + ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    uint8_t *out = (uint8_t *)buffer;

    for (uint32_t sector = 0; sector < count; sector++) {
        if (!ata_wait_drq(io_base)) {
            log_error("ATA read: DRQ timeout/error");
            return false;
        }

        ata_read_words(io_base, (uint16_t *)(out + sector * ATA_SECTOR_SIZE), 256);
        ata_delay_400ns(ctrl_base);
    }

    return true;
}

static void ata_write_words(uint16_t io_base, const uint16_t *buffer, uint32_t words) {
    for (uint32_t i = 0; i < words; i++) {
        outw(io_base + ATA_REG_DATA, buffer[i]);
    }
}

static bool ata_cache_flush(ata_device_t *dev) {
    uint16_t io_base = dev->io_base;
    uint16_t ctrl_base = dev->ctrl_base;

    if (!ata_wait_not_busy(io_base)) {
        return false;
    }

    outb(io_base + ATA_REG_HDDEVSEL, (uint8_t)(0xE0 | (dev->slave << 4)));
    ata_delay_400ns(ctrl_base);

    outb(io_base + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);

    if (!ata_wait_not_busy(io_base)) {
        return false;
    }

    uint8_t status = ata_status(io_base);

    if (status & ATA_SR_ERR) {
        return false;
    }

    if (status & ATA_SR_DF) {
        return false;
    }

    return true;
}

bool ata_write_sector(ata_drive_t drive, uint32_t lba, const void *buffer) {
    return ata_write_sectors(drive, lba, 1, buffer);
}

bool ata_write_sectors(ata_drive_t drive, uint32_t lba, uint8_t count, const void *buffer) {
    if ((uint32_t)drive >= 4 || buffer == NULL || count == 0) {
        return false;
    }

    ata_device_t *dev = &ata_devices[drive];

    if (!dev->present) {
        log_error("ATA write: device not present");
        return false;
    }

    if (lba + count > dev->sectors_28) {
        log_error("ATA write: LBA out of range");
        return false;
    }

    uint16_t io_base = dev->io_base;
    uint16_t ctrl_base = dev->ctrl_base;

    if (!ata_wait_not_busy(io_base)) {
        log_error("ATA write: busy timeout");
        return false;
    }

    outb(
        io_base + ATA_REG_HDDEVSEL,
        (uint8_t)(0xE0 | (dev->slave << 4) | ((lba >> 24) & 0x0F))
    );

    ata_delay_400ns(ctrl_base);

    outb(io_base + ATA_REG_SECCOUNT0, count);
    outb(io_base + ATA_REG_LBA0, (uint8_t)(lba & 0xFF));
    outb(io_base + ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFF));
    outb(io_base + ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

    const uint8_t *in = (const uint8_t *)buffer;

    for (uint32_t sector = 0; sector < count; sector++) {
        if (!ata_wait_drq(io_base)) {
            log_error("ATA write: DRQ timeout/error");
            return false;
        }

        ata_write_words(
            io_base,
            (const uint16_t *)(in + sector * ATA_SECTOR_SIZE),
            256
        );

        ata_delay_400ns(ctrl_base);
    }

    if (!ata_cache_flush(dev)) {
        log_error("ATA write: cache flush failed");
        return false;
    }

    return true;
}

static uint8_t write_sector[512] __attribute__((aligned(2)));
static uint8_t read_sector[512] __attribute__((aligned(2)));

void ata_test_write(void) {
    static uint8_t write_sector[512] __attribute__((aligned(2)));
    static uint8_t read_sector[512] __attribute__((aligned(2)));

    for (uint32_t i = 0; i < 512; i++) {
        write_sector[i] = 0;
        read_sector[i] = 0;
    }

    write_sector[0] = 'A';
    write_sector[1] = 'S';
    write_sector[2] = 'T';
    write_sector[3] = 'R';

    if (!ata_write_sector(ATA_PRIMARY_MASTER, 2048, write_sector)) {
        log_error("ATA write test failed");
        return;
    }

    log_ok("ATA write test OK");

    if (!ata_read_sector(ATA_PRIMARY_MASTER, 2048, read_sector)) {
        log_error("ATA readback failed");
        return;
    }

    log_ok("ATA readback OK");

    log_infof(
        "readback magic: %c%c%c%c",
        read_sector[0],
        read_sector[1],
        read_sector[2],
        read_sector[3]
    );
}
