#include "disk.h"

static disk_t primary_disk;

// I/O port access functions
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void insw(uint16_t port, void* buffer, uint32_t count) {
    __asm__ volatile ("rep insw" : "+D"(buffer), "+c"(count) : "d"(port) : "memory");
}

static inline void outsw(uint16_t port, const void* buffer, uint32_t count) {
    __asm__ volatile ("rep outsw" : "+S"(buffer), "+c"(count) : "d"(port) : "memory");
}

static int _disk_wait_ready(uint16_t base_port) {
    uint32_t timeout = DISK_TIMEOUT;
    uint8_t status;
    
    while (timeout--) {
        status = inb(base_port + ATA_REG_STATUS);
        if (!(status & ATA_STATUS_BSY) && (status & ATA_STATUS_DRDY)) {
            return DISK_SUCCESS;
        }
    }
    return DISK_TIMEOUT_ERR;
}

static int _disk_wait_data(uint16_t base_port) {
    uint32_t timeout = DISK_TIMEOUT;
    uint8_t status;
    
    while (timeout--) {
        status = inb(base_port + ATA_REG_STATUS);
        if (!(status & ATA_STATUS_BSY) && (status & ATA_STATUS_DRQ)) {
            return DISK_SUCCESS;
        }
        if (status & ATA_STATUS_ERR) {
            return DISK_ERROR;
        }
    }
    return DISK_TIMEOUT_ERR;
}

static void _disk_select_drive(uint16_t base_port, uint8_t drive) {
    outb(base_port + ATA_REG_DRIVE, 0xE0 | (drive << 4));
    // Small delay after drive selection
    for (int i = 0; i < 4; i++) {
        inb(base_port + ATA_REG_STATUS);
    }
}

static uint8_t _disk_read_status(uint16_t base_port) {
    return inb(base_port + ATA_REG_STATUS);
}

int disk_init(void) {
    // Initialize primary disk
    primary_disk.base_port = ATA_PRIMARY_BASE;
    primary_disk.ctrl_port = ATA_PRIMARY_CTRL;
    primary_disk.drive_num = 0;  // Master drive
    primary_disk.present = false;
    primary_disk.total_sectors = 0;
    
    // Select the primary master drive
    _disk_select_drive(primary_disk.base_port, primary_disk.drive_num);
    
    // Wait for drive to be ready
    if (_disk_wait_ready(primary_disk.base_port) != DISK_SUCCESS) {
        return DISK_NOT_READY;
    }
    
    // Send IDENTIFY command
    outb(primary_disk.base_port + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    
    // Check if drive exists
    uint8_t status = _disk_read_status(primary_disk.base_port);
    if (status == 0) {
        return DISK_ERROR;  // No drive
    }
    
    // Wait for data to be ready
    if (_disk_wait_data(primary_disk.base_port) != DISK_SUCCESS) {
        return DISK_ERROR;
    }
    
    // Read identify data
    uint16_t identify_data[256];
    insw(primary_disk.base_port + ATA_REG_DATA, identify_data, 256);
    
    // Extract total sectors (assuming LBA28 for simplicity)
    primary_disk.total_sectors = (uint32_t)identify_data[60] | ((uint32_t)identify_data[61] << 16);
    primary_disk.present = true;
    
    return DISK_SUCCESS;
}

int disk_read_sector(uint32_t lba, void* buffer) {
    if (!primary_disk.present) {
        return DISK_ERROR;
    }
    
    if (lba >= primary_disk.total_sectors) {
        return DISK_ERROR;
    }
    
    // Wait for drive to be ready
    if (_disk_wait_ready(primary_disk.base_port) != DISK_SUCCESS) {
        return DISK_TIMEOUT_ERR;
    }
    
    // Select drive and set LBA mode
    _disk_select_drive(primary_disk.base_port, primary_disk.drive_num);
    outb(primary_disk.base_port + ATA_REG_DRIVE, 0xE0 | (primary_disk.drive_num << 4) | ((lba >> 24) & 0x0F));
    
    // Set sector count and LBA
    outb(primary_disk.base_port + ATA_REG_SECCOUNT, 1);
    outb(primary_disk.base_port + ATA_REG_LBA_LOW, lba & 0xFF);
    outb(primary_disk.base_port + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(primary_disk.base_port + ATA_REG_LBA_HIGH, (lba >> 16) & 0xFF);
    
    // Send read command
    outb(primary_disk.base_port + ATA_REG_COMMAND, ATA_CMD_READ_SECTORS);
    
    // Wait for data to be ready
    if (_disk_wait_data(primary_disk.base_port) != DISK_SUCCESS) {
        return DISK_ERROR;
    }
    
    // Read the sector data
    insw(primary_disk.base_port + ATA_REG_DATA, buffer, SECTOR_SIZE / 2);
    
    return DISK_SUCCESS;
}

int disk_write_sector(uint32_t lba, const void* buffer) {
    if (!primary_disk.present) {
        return DISK_ERROR;
    }
    
    if (lba >= primary_disk.total_sectors) {
        return DISK_ERROR;
    }
    
    // Wait for drive to be ready
    if (_disk_wait_ready(primary_disk.base_port) != DISK_SUCCESS) {
        return DISK_TIMEOUT_ERR;
    }
    
    // Select drive and set LBA mode
    _disk_select_drive(primary_disk.base_port, primary_disk.drive_num);
    outb(primary_disk.base_port + ATA_REG_DRIVE, 0xE0 | (primary_disk.drive_num << 4) | ((lba >> 24) & 0x0F));
    
    // Set sector count and LBA
    outb(primary_disk.base_port + ATA_REG_SECCOUNT, 1);
    outb(primary_disk.base_port + ATA_REG_LBA_LOW, lba & 0xFF);
    outb(primary_disk.base_port + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(primary_disk.base_port + ATA_REG_LBA_HIGH, (lba >> 16) & 0xFF);
    
    // Send write command
    outb(primary_disk.base_port + ATA_REG_COMMAND, ATA_CMD_WRITE_SECTORS);
    
    // Wait for drive to be ready for data
    if (_disk_wait_data(primary_disk.base_port) != DISK_SUCCESS) {
        return DISK_ERROR;
    }
    
    // Write the sector data
    outsw(primary_disk.base_port + ATA_REG_DATA, buffer, SECTOR_SIZE / 2);
    
    // Wait for write to complete
    if (_disk_wait_ready(primary_disk.base_port) != DISK_SUCCESS) {
        return DISK_ERROR;
    }
    
    return DISK_SUCCESS;
}

int disk_read_sectors(uint32_t lba, uint32_t count, void* buffer) {
    char* buf = (char*)buffer;
    
    for (uint32_t i = 0; i < count; i++) {
        int result = disk_read_sector(lba + i, buf + (i * SECTOR_SIZE));
        if (result != DISK_SUCCESS) {
            return result;
        }
    }
    
    return DISK_SUCCESS;
}

int disk_write_sectors(uint32_t lba, uint32_t count, const void* buffer) {
    const char* buf = (const char*)buffer;
    
    for (uint32_t i = 0; i < count; i++) {
        int result = disk_write_sector(lba + i, buf + (i * SECTOR_SIZE));
        if (result != DISK_SUCCESS) {
            return result;
        }
    }
    
    return DISK_SUCCESS;
}

uint32_t disk_get_total_sectors(void) {
    return primary_disk.total_sectors;
}

bool disk_is_present(void) {
    return primary_disk.present;
}