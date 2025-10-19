#ifndef DISK_H
#define DISK_H

#include "../types.h"

// Disk constants
#define SECTOR_SIZE 512
#define DISK_TIMEOUT 1000000  // Timeout for disk operations

// Disk status codes
#define DISK_SUCCESS    0
#define DISK_ERROR      -1
#define DISK_TIMEOUT_ERR -2
#define DISK_NOT_READY  -3

// ATA/IDE port definitions
#define ATA_PRIMARY_BASE    0x1F0
#define ATA_PRIMARY_CTRL    0x3F6
#define ATA_SECONDARY_BASE  0x170
#define ATA_SECONDARY_CTRL  0x376

// ATA register offsets
#define ATA_REG_DATA       0x00
#define ATA_REG_ERROR      0x01
#define ATA_REG_FEATURES   0x01
#define ATA_REG_SECCOUNT   0x02
#define ATA_REG_LBA_LOW    0x03
#define ATA_REG_LBA_MID    0x04
#define ATA_REG_LBA_HIGH   0x05
#define ATA_REG_DRIVE      0x06
#define ATA_REG_STATUS     0x07
#define ATA_REG_COMMAND    0x07

// ATA status register bits
#define ATA_STATUS_BSY     0x80  // Busy
#define ATA_STATUS_DRDY    0x40  // Drive ready
#define ATA_STATUS_DRQ     0x08  // Data request
#define ATA_STATUS_ERR     0x01  // Error

// ATA commands
#define ATA_CMD_READ_SECTORS  0x20
#define ATA_CMD_WRITE_SECTORS 0x30
#define ATA_CMD_IDENTIFY      0xEC

// Disk structure
typedef struct {
    uint16_t base_port;       // Base I/O port
    uint16_t ctrl_port;       // Control port
    uint8_t drive_num;        // Drive number (0 = master, 1 = slave)
    uint32_t total_sectors;   // Total number of sectors
    bool present;             // Whether disk is present
} disk_t;

// Function prototypes
int disk_init(void);
int disk_read_sector(uint32_t lba, void* buffer);
int disk_write_sector(uint32_t lba, const void* buffer);
int disk_read_sectors(uint32_t lba, uint32_t count, void* buffer);
int disk_write_sectors(uint32_t lba, uint32_t count, const void* buffer);
uint32_t disk_get_total_sectors(void);
bool disk_is_present(void);

// Internal helper functions (defined in disk.c)


#endif // DISK_H