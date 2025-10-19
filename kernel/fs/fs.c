#include "fs.h"
#include "disk.h"
#include "../mm/memory.h"
#include "../drivers/vga.h"

// File system constants
#define FS_MAGIC 0x54524144  // "TRAD" - TradeKernel filesystem magic

// Global file system state
static bool fs_mounted = false;
static superblock_t superblock;
static file_descriptor_t file_descriptors[MAX_OPEN_FILES];
static uint8_t* block_bitmap = NULL;    // Block allocation bitmap
static uint8_t* inode_bitmap = NULL;    // Inode allocation bitmap

// Forward declarations for internal functions
static int find_directory_entry(uint32_t dir_inode_num, const char* name, directory_entry_t* entry);
static int resolve_path(const char* path, uint32_t* inode_num);
static int add_directory_entry(uint32_t dir_inode_num, const char* name, uint32_t entry_inode_num, uint8_t file_type);

// Helper function to convert block number to LBA
static uint32_t block_to_lba(uint32_t block_num) {
    return block_num * (BLOCK_SIZE / SECTOR_SIZE);
}

// Read a block from disk
static int read_block(uint32_t block_num, void* buffer) {
    if (block_num >= superblock.total_blocks) {
        return FS_ERROR_INVALID;
    }
    
    uint32_t lba = block_to_lba(block_num);
    uint32_t sectors = BLOCK_SIZE / SECTOR_SIZE;
    
    return disk_read_sectors(lba, sectors, buffer);
}

// Write a block to disk
static int write_block(uint32_t block_num, const void* buffer) {
    if (block_num >= superblock.total_blocks) {
        return FS_ERROR_INVALID;
    }
    
    uint32_t lba = block_to_lba(block_num);
    uint32_t sectors = BLOCK_SIZE / SECTOR_SIZE;
    
    return disk_write_sectors(lba, sectors, buffer);
}

int _fs_read_superblock(superblock_t* sb) {
    return read_block(0, sb);
}

int _fs_write_superblock(const superblock_t* sb) {
    return write_block(0, sb);
}

int _fs_read_inode(uint32_t inode_num, inode_t* inode) {
    if (inode_num == 0 || inode_num > superblock.total_inodes) {
        return FS_ERROR_INVALID;
    }
    
    // Calculate which block contains this inode
    uint32_t inodes_per_block = BLOCK_SIZE / sizeof(inode_t);
    uint32_t block_num = 1 + ((inode_num - 1) / inodes_per_block);  // Skip superblock
    uint32_t inode_offset = (inode_num - 1) % inodes_per_block;
    
    // Read the block containing the inode
    uint8_t block_buffer[BLOCK_SIZE];
    int result = read_block(block_num, block_buffer);
    if (result != DISK_SUCCESS) {
        return FS_ERROR_INVALID;
    }
    
    // Copy the inode data
    inode_t* inodes = (inode_t*)block_buffer;
    *inode = inodes[inode_offset];
    
    return FS_SUCCESS;
}

int _fs_write_inode(uint32_t inode_num, const inode_t* inode) {
    if (inode_num == 0 || inode_num > superblock.total_inodes) {
        return FS_ERROR_INVALID;
    }
    
    // Calculate which block contains this inode
    uint32_t inodes_per_block = BLOCK_SIZE / sizeof(inode_t);
    uint32_t block_num = 1 + ((inode_num - 1) / inodes_per_block);  // Skip superblock
    uint32_t inode_offset = (inode_num - 1) % inodes_per_block;
    
    // Read the block containing the inode
    uint8_t block_buffer[BLOCK_SIZE];
    int result = read_block(block_num, block_buffer);
    if (result != DISK_SUCCESS) {
        return FS_ERROR_INVALID;
    }
    
    // Update the inode data
    inode_t* inodes = (inode_t*)block_buffer;
    inodes[inode_offset] = *inode;
    
    // Write the block back
    return write_block(block_num, block_buffer);
}

int _fs_allocate_block(void) {
    if (!block_bitmap) {
        return FS_ERROR_INVALID;
    }
    
    // Find first free block
    for (uint32_t i = 0; i < superblock.total_blocks; i++) {
        uint32_t byte_index = i / 8;
        uint32_t bit_index = i % 8;
        
        if (!(block_bitmap[byte_index] & (1 << bit_index))) {
            // Mark block as used
            block_bitmap[byte_index] |= (1 << bit_index);
            superblock.free_blocks--;
            return i;
        }
    }
    
    return FS_ERROR_NO_SPACE;
}

int _fs_free_block(uint32_t block_num) {
    if (!block_bitmap || block_num >= superblock.total_blocks) {
        return FS_ERROR_INVALID;
    }
    
    uint32_t byte_index = block_num / 8;
    uint32_t bit_index = block_num % 8;
    
    // Mark block as free
    block_bitmap[byte_index] &= ~(1 << bit_index);
    superblock.free_blocks++;
    
    return FS_SUCCESS;
}

int _fs_allocate_inode(void) {
    if (!inode_bitmap) {
        return FS_ERROR_INVALID;
    }
    
    // Find first free inode (starting from 1, since 0 is invalid)
    for (uint32_t i = 1; i <= superblock.total_inodes; i++) {
        uint32_t byte_index = i / 8;
        uint32_t bit_index = i % 8;
        
        if (!(inode_bitmap[byte_index] & (1 << bit_index))) {
            // Mark inode as used
            inode_bitmap[byte_index] |= (1 << bit_index);
            superblock.free_inodes--;
            return i;
        }
    }
    
    return FS_ERROR_NO_SPACE;
}

int _fs_free_inode(uint32_t inode_num) {
    if (!inode_bitmap || inode_num == 0 || inode_num > superblock.total_inodes) {
        return FS_ERROR_INVALID;
    }
    
    uint32_t byte_index = inode_num / 8;
    uint32_t bit_index = inode_num % 8;
    
    // Mark inode as free
    inode_bitmap[byte_index] &= ~(1 << bit_index);
    superblock.free_inodes++;
    
    return FS_SUCCESS;
}

int fs_init(void) {
    // Initialize disk
    vga_write_string("Initializing disk...\n");
    if (disk_init() != DISK_SUCCESS) {
        vga_write_string("Disk initialization failed!\n");
        return FS_ERROR_INVALID;
    }
    vga_write_string("Disk initialized successfully.\n");
    
    // Initialize file descriptor table
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        file_descriptors[i].in_use = false;
    }
    
    // Try to read existing superblock
    if (_fs_read_superblock(&superblock) == DISK_SUCCESS && 
        superblock.magic == FS_MAGIC) {
        
        // Valid filesystem found, load bitmaps
        uint32_t bitmap_blocks = (superblock.total_blocks + 7) / 8 / BLOCK_SIZE + 1;
        
        block_bitmap = (uint8_t*)kmalloc(bitmap_blocks * BLOCK_SIZE);
        inode_bitmap = (uint8_t*)kmalloc(bitmap_blocks * BLOCK_SIZE);
        
        if (!block_bitmap || !inode_bitmap) {
            return FS_ERROR_NO_MEMORY;
        }
        
        // Read bitmaps from disk (simplified - assume they follow inode blocks)
        uint32_t bitmap_start = 1 + superblock.inode_blocks;
        read_block(bitmap_start, block_bitmap);
        read_block(bitmap_start + 1, inode_bitmap);
        
        fs_mounted = true;
        return FS_SUCCESS;
    }
    
    // No valid filesystem, need to format
    return FS_ERROR_NOT_FOUND;
}

int fs_format(void) {
    vga_write_string("Formatting filesystem...\n");
    uint32_t total_sectors = disk_get_total_sectors();
    vga_write_string("Total sectors: ");
    // Print total sectors
    char sector_str[20];
    int temp = total_sectors;
    int i = 0;
    do {
        sector_str[i++] = '0' + (temp % 10);
        temp /= 10;
    } while (temp > 0 && i < 19);
    while (i > 0) {
        vga_putchar(sector_str[--i]);
    }
    vga_write_string("\n");
    
    if (total_sectors == 0) {
        vga_write_string("No disk sectors available!\n");
        return FS_ERROR_INVALID;
    }
    
    // Calculate filesystem layout
    uint32_t total_blocks = total_sectors / (BLOCK_SIZE / SECTOR_SIZE);
    uint32_t inodes_per_block = BLOCK_SIZE / sizeof(inode_t);
    uint32_t total_inodes = total_blocks / 4;  // 25% of blocks for inodes
    uint32_t inode_blocks = (total_inodes + inodes_per_block - 1) / inodes_per_block;
    uint32_t bitmap_blocks = 2;  // One for block bitmap, one for inode bitmap
    uint32_t data_blocks = total_blocks - 1 - inode_blocks - bitmap_blocks;  // Subtract superblock
    
    // Initialize superblock
    superblock.magic = FS_MAGIC;
    superblock.total_blocks = total_blocks;
    superblock.free_blocks = data_blocks;
    superblock.inode_blocks = inode_blocks;
    superblock.data_blocks = data_blocks;
    superblock.block_size = BLOCK_SIZE;
    superblock.inodes_per_block = inodes_per_block;
    superblock.total_inodes = total_inodes;
    superblock.free_inodes = total_inodes - 1;  // Reserve root directory inode
    superblock.root_inode = ROOT_INODE;
    
    // Write superblock
    if (_fs_write_superblock(&superblock) != DISK_SUCCESS) {
        return FS_ERROR_INVALID;
    }
    
    // Initialize and allocate bitmaps
    block_bitmap = (uint8_t*)kmalloc(BLOCK_SIZE);
    inode_bitmap = (uint8_t*)kmalloc(BLOCK_SIZE);
    
    if (!block_bitmap || !inode_bitmap) {
        return FS_ERROR_NO_MEMORY;
    }
    
    // Clear bitmaps
    memset(block_bitmap, 0, BLOCK_SIZE);
    memset(inode_bitmap, 0, BLOCK_SIZE);
    
    // Mark system blocks as used
    for (uint32_t i = 0; i < 1 + inode_blocks + bitmap_blocks; i++) {
        uint32_t byte_index = i / 8;
        uint32_t bit_index = i % 8;
        block_bitmap[byte_index] |= (1 << bit_index);
    }
    
    // Mark root inode as used
    inode_bitmap[ROOT_INODE / 8] |= (1 << (ROOT_INODE % 8));
    
    // Write bitmaps to disk
    uint32_t bitmap_start = 1 + inode_blocks;
    write_block(bitmap_start, block_bitmap);
    write_block(bitmap_start + 1, inode_bitmap);
    
    // Create root directory inode
    inode_t root_inode;
    memset(&root_inode, 0, sizeof(inode_t));
    root_inode.inode_num = ROOT_INODE;
    root_inode.file_type = FILE_TYPE_DIRECTORY;
    root_inode.permissions = PERM_READ | PERM_WRITE | PERM_EXECUTE;
    root_inode.size = 0;
    root_inode.blocks_used = 0;
    
    _fs_write_inode(ROOT_INODE, &root_inode);
    
    fs_mounted = true;
    return FS_SUCCESS;
}

uint32_t fs_get_free_space(void) {
    if (!fs_mounted) {
        return 0;
    }
    return superblock.free_blocks * BLOCK_SIZE;
}

int fs_exists(const char* path) {
    if (!fs_mounted || !path) {
        return false;
    }
    
    inode_t inode;
    return fs_stat(path, &inode) == FS_SUCCESS;
}

// Helper function to find a directory entry by name
static int find_directory_entry(uint32_t dir_inode_num, const char* name, directory_entry_t* entry) {
    inode_t dir_inode;
    if (_fs_read_inode(dir_inode_num, &dir_inode) != FS_SUCCESS) {
        return FS_ERROR_INVALID;
    }
    
    if (dir_inode.file_type != FILE_TYPE_DIRECTORY) {
        return FS_ERROR_INVALID;
    }
    
    // Read directory blocks and search for the entry
    uint8_t block_buffer[BLOCK_SIZE];
    uint32_t entries_per_block = BLOCK_SIZE / sizeof(directory_entry_t);
    
    for (uint32_t i = 0; i < dir_inode.blocks_used; i++) {
        if (i >= 12) break; // Only handle direct blocks for now
        
        uint32_t block_num = dir_inode.direct_blocks[i];
        if (block_num == 0) continue;
        
        if (read_block(block_num, block_buffer) != DISK_SUCCESS) {
            continue;
        }
        
        directory_entry_t* entries = (directory_entry_t*)block_buffer;
        for (uint32_t j = 0; j < entries_per_block; j++) {
            if (entries[j].inode_num == 0) continue; // Empty entry
            
            // Compare names (simple string comparison)
            int match = 1;
            for (int k = 0; k < MAX_FILENAME_LENGTH && name[k] && entries[j].name[k]; k++) {
                if (name[k] != entries[j].name[k]) {
                    match = 0;
                    break;
                }
            }
            
            if (match && name[entries[j].name_length] == '\0') {
                *entry = entries[j];
                return FS_SUCCESS;
            }
        }
    }
    
    return FS_ERROR_NOT_FOUND;
}

// Simple path resolution - handles paths like "/", "/dir", "/dir/file"
static int resolve_path(const char* path, uint32_t* inode_num) {
    if (!path || path[0] != '/') {
        return FS_ERROR_INVALID;
    }
    
    // Root directory
    if (path[1] == '\0') {
        *inode_num = ROOT_INODE;
        return FS_SUCCESS;
    }
    
    // Parse path components
    uint32_t current_inode = ROOT_INODE;
    const char* component = path + 1; // Skip leading slash
    
    while (*component) {
        // Find end of current component
        const char* end = component;
        while (*end && *end != '/') end++;
        
        // Copy component name
        char name[MAX_FILENAME_LENGTH];
        int len = end - component;
        if (len >= MAX_FILENAME_LENGTH) {
            return FS_ERROR_INVALID;
        }
        
        for (int i = 0; i < len; i++) {
            name[i] = component[i];
        }
        name[len] = '\0';
        
        // Find directory entry
        directory_entry_t entry;
        if (find_directory_entry(current_inode, name, &entry) != FS_SUCCESS) {
            return FS_ERROR_NOT_FOUND;
        }
        
        current_inode = entry.inode_num;
        
        // Move to next component
        component = end;
        if (*component == '/') component++;
    }
    
    *inode_num = current_inode;
    return FS_SUCCESS;
}

int fs_stat(const char* path, inode_t* stat_info) {
    if (!fs_mounted || !path || !stat_info) {
        return FS_ERROR_INVALID;
    }
    
    uint32_t inode_num;
    if (resolve_path(path, &inode_num) != FS_SUCCESS) {
        return FS_ERROR_NOT_FOUND;
    }
    
    return _fs_read_inode(inode_num, stat_info);
}

int fs_create_directory(const char* path) {
    if (!fs_mounted || !path) {
        return FS_ERROR_INVALID;
    }
    
    // Find parent directory
    char parent_path[MAX_PATH_LENGTH];
    char dir_name[MAX_FILENAME_LENGTH];
    
    // Parse path to get parent and directory name
    int path_len = 0;
    while (path[path_len]) path_len++; // Get string length
    
    int last_slash = -1;
    for (int i = path_len - 1; i >= 0; i--) {
        if (path[i] == '/') {
            last_slash = i;
            break;
        }
    }
    
    if (last_slash == -1) {
        // No slash found - create in root directory
        parent_path[0] = '/';
        parent_path[1] = '\0';
        
        // Copy directory name
        int name_len = 0;
        for (int i = 0; path[i] && name_len < MAX_FILENAME_LENGTH - 1; i++) {
            dir_name[name_len++] = path[i];
        }
        dir_name[name_len] = '\0';
    } else {
        // Copy parent path
        for (int i = 0; i < last_slash; i++) {
            parent_path[i] = path[i];
        }
        if (last_slash == 0) {
            parent_path[0] = '/';
            parent_path[1] = '\0';
        } else {
            parent_path[last_slash] = '\0';
        }
        
        // Copy directory name
        int name_len = 0;
        for (int i = last_slash + 1; path[i] && name_len < MAX_FILENAME_LENGTH - 1; i++) {
            dir_name[name_len++] = path[i];
        }
        dir_name[name_len] = '\0';
    }
    
    // Get parent inode
    uint32_t parent_inode_num;
    if (resolve_path(parent_path, &parent_inode_num) != FS_SUCCESS) {
        // If parent is root and path resolution failed, use root inode directly
        if (parent_path[0] == '/' && parent_path[1] == '\0') {
            parent_inode_num = ROOT_INODE;
        } else {
            return FS_ERROR_NOT_FOUND;
        }
    }
    
    // Check if directory already exists
    directory_entry_t existing_entry;
    if (find_directory_entry(parent_inode_num, dir_name, &existing_entry) == FS_SUCCESS) {
        return FS_ERROR_EXISTS;
    }
    
    // Allocate new inode
    int new_inode_num = _fs_allocate_inode();
    if (new_inode_num < 0) {
        return new_inode_num;
    }
    
    // Create directory inode
    inode_t dir_inode;
    memset(&dir_inode, 0, sizeof(inode_t));
    dir_inode.inode_num = new_inode_num;
    dir_inode.file_type = FILE_TYPE_DIRECTORY;
    dir_inode.permissions = PERM_READ | PERM_WRITE | PERM_EXECUTE;
    dir_inode.size = 0;
    dir_inode.blocks_used = 0;
    
    // Write the new inode
    if (_fs_write_inode(new_inode_num, &dir_inode) != FS_SUCCESS) {
        _fs_free_inode(new_inode_num);
        return FS_ERROR_INVALID;
    }
    
    // Add entry to parent directory
    return add_directory_entry(parent_inode_num, dir_name, new_inode_num, FILE_TYPE_DIRECTORY);
}

// Helper function to add a directory entry
static int add_directory_entry(uint32_t dir_inode_num, const char* name, uint32_t entry_inode_num, uint8_t file_type) {
    inode_t dir_inode;
    if (_fs_read_inode(dir_inode_num, &dir_inode) != FS_SUCCESS) {
        return FS_ERROR_INVALID;
    }
    
    if (dir_inode.file_type != FILE_TYPE_DIRECTORY) {
        return FS_ERROR_INVALID;
    }
    
    // Find a free slot in existing blocks or allocate a new block
    uint8_t block_buffer[BLOCK_SIZE];
    uint32_t entries_per_block = BLOCK_SIZE / sizeof(directory_entry_t);
    
    // Search existing blocks for free slot
    for (uint32_t i = 0; i < dir_inode.blocks_used && i < 12; i++) {
        uint32_t block_num = dir_inode.direct_blocks[i];
        if (block_num == 0) continue;
        
        if (read_block(block_num, block_buffer) != DISK_SUCCESS) {
            continue;
        }
        
        directory_entry_t* entries = (directory_entry_t*)block_buffer;
        for (uint32_t j = 0; j < entries_per_block; j++) {
            if (entries[j].inode_num == 0) { // Free slot found
                entries[j].inode_num = entry_inode_num;
                entries[j].name_length = 0;
                while (name[entries[j].name_length] && entries[j].name_length < MAX_FILENAME_LENGTH - 1) {
                    entries[j].name[entries[j].name_length] = name[entries[j].name_length];
                    entries[j].name_length++;
                }
                entries[j].name[entries[j].name_length] = '\0';
                entries[j].file_type = file_type;
                
                // Write block back
                write_block(block_num, block_buffer);
                dir_inode.size += sizeof(directory_entry_t);
                _fs_write_inode(dir_inode_num, &dir_inode);
                return FS_SUCCESS;
            }
        }
    }
    
    // Need to allocate a new block
    if (dir_inode.blocks_used >= 12) {
        return FS_ERROR_NO_SPACE; // Directory full (no indirect blocks yet)
    }
    
    int new_block = _fs_allocate_block();
    if (new_block < 0) {
        return new_block;
    }
    
    // Initialize new block
    memset(block_buffer, 0, BLOCK_SIZE);
    directory_entry_t* entries = (directory_entry_t*)block_buffer;
    
    // Add the entry
    entries[0].inode_num = entry_inode_num;
    entries[0].name_length = 0;
    while (name[entries[0].name_length] && entries[0].name_length < MAX_FILENAME_LENGTH - 1) {
        entries[0].name[entries[0].name_length] = name[entries[0].name_length];
        entries[0].name_length++;
    }
    entries[0].name[entries[0].name_length] = '\0';
    entries[0].file_type = file_type;
    
    // Write the new block
    write_block(new_block, block_buffer);
    
    // Update directory inode
    dir_inode.direct_blocks[dir_inode.blocks_used] = new_block;
    dir_inode.blocks_used++;
    dir_inode.size += sizeof(directory_entry_t);
    _fs_write_inode(dir_inode_num, &dir_inode);
    
    return FS_SUCCESS;
}

int fs_list_directory(const char* path, directory_entry_t* entries, uint32_t max_entries) {
    if (!fs_mounted || !path || !entries) {
        return FS_ERROR_INVALID;
    }
    
    uint32_t dir_inode_num;
    if (resolve_path(path, &dir_inode_num) != FS_SUCCESS) {
        return FS_ERROR_NOT_FOUND;
    }
    
    inode_t dir_inode;
    if (_fs_read_inode(dir_inode_num, &dir_inode) != FS_SUCCESS) {
        return FS_ERROR_INVALID;
    }
    
    if (dir_inode.file_type != FILE_TYPE_DIRECTORY) {
        return FS_ERROR_INVALID;
    }
    
    uint32_t entry_count = 0;
    uint8_t block_buffer[BLOCK_SIZE];
    uint32_t entries_per_block = BLOCK_SIZE / sizeof(directory_entry_t);
    
    // Read all directory blocks
    for (uint32_t i = 0; i < dir_inode.blocks_used && i < 12; i++) {
        uint32_t block_num = dir_inode.direct_blocks[i];
        if (block_num == 0) continue;
        
        if (read_block(block_num, block_buffer) != DISK_SUCCESS) {
            continue;
        }
        
        directory_entry_t* block_entries = (directory_entry_t*)block_buffer;
        for (uint32_t j = 0; j < entries_per_block && entry_count < max_entries; j++) {
            if (block_entries[j].inode_num != 0) { // Valid entry
                entries[entry_count] = block_entries[j];
                entry_count++;
            }
        }
    }
    
    return entry_count;
}

int fs_create_file(const char* path, uint8_t file_type) {
    if (!fs_mounted || !path) {
        return FS_ERROR_INVALID;
    }
    
    // Parse path to get parent directory and filename
    char parent_path[MAX_PATH_LENGTH];
    char filename[MAX_FILENAME_LENGTH];
    
    int path_len = 0;
    while (path[path_len]) path_len++;
    
    int last_slash = -1;
    for (int i = path_len - 1; i >= 0; i--) {
        if (path[i] == '/') {
            last_slash = i;
            break;
        }
    }
    
    if (last_slash == -1) {
        return FS_ERROR_INVALID;
    }
    
    // Copy parent path
    for (int i = 0; i < last_slash; i++) {
        parent_path[i] = path[i];
    }
    if (last_slash == 0) {
        parent_path[0] = '/';
        parent_path[1] = '\0';
    } else {
        parent_path[last_slash] = '\0';
    }
    
    // Copy filename
    int name_len = 0;
    for (int i = last_slash + 1; path[i] && name_len < MAX_FILENAME_LENGTH - 1; i++) {
        filename[name_len++] = path[i];
    }
    filename[name_len] = '\0';
    
    // Get parent directory inode
    uint32_t parent_inode_num;
    if (resolve_path(parent_path, &parent_inode_num) != FS_SUCCESS) {
        return FS_ERROR_NOT_FOUND;
    }
    
    // Check if file already exists
    directory_entry_t existing_entry;
    if (find_directory_entry(parent_inode_num, filename, &existing_entry) == FS_SUCCESS) {
        return FS_ERROR_EXISTS;
    }
    
    // Allocate new inode
    int new_inode_num = _fs_allocate_inode();
    if (new_inode_num < 0) {
        return new_inode_num;
    }
    
    // Create file inode
    inode_t file_inode;
    memset(&file_inode, 0, sizeof(inode_t));
    file_inode.inode_num = new_inode_num;
    file_inode.file_type = file_type;
    file_inode.permissions = PERM_READ | PERM_WRITE;
    file_inode.size = 0;
    file_inode.blocks_used = 0;
    
    // Write the new inode
    if (_fs_write_inode(new_inode_num, &file_inode) != FS_SUCCESS) {
        _fs_free_inode(new_inode_num);
        return FS_ERROR_INVALID;
    }
    
    // Add entry to parent directory
    return add_directory_entry(parent_inode_num, filename, new_inode_num, file_type);
}

int fs_delete_file(const char* path) {
    if (!fs_mounted || !path) {
        return FS_ERROR_INVALID;
    }
    
    uint32_t inode_num;
    if (resolve_path(path, &inode_num) != FS_SUCCESS) {
        return FS_ERROR_NOT_FOUND;
    }
    
    inode_t inode;
    if (_fs_read_inode(inode_num, &inode) != FS_SUCCESS) {
        return FS_ERROR_INVALID;
    }
    
    // Free all blocks used by the file
    for (uint32_t i = 0; i < inode.blocks_used && i < 12; i++) {
        if (inode.direct_blocks[i] != 0) {
            _fs_free_block(inode.direct_blocks[i]);
        }
    }
    
    // Free the inode
    _fs_free_inode(inode_num);
    
    // TODO: Remove directory entry from parent directory
    // This would require additional logic to find and remove the entry
    
    return FS_SUCCESS;
}

// Simple file operations (basic implementation)
int fs_open(const char* path, uint8_t flags) {
    if (!fs_mounted || !path) {
        return FS_ERROR_INVALID;
    }
    
    // Find free file descriptor
    int fd = -1;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!file_descriptors[i].in_use) {
            fd = i;
            break;
        }
    }
    
    if (fd == -1) {
        return FS_ERROR_NO_SPACE;
    }
    
    uint32_t inode_num;
    if (resolve_path(path, &inode_num) != FS_SUCCESS) {
        return FS_ERROR_NOT_FOUND;
    }
    
    // Read inode
    if (_fs_read_inode(inode_num, &file_descriptors[fd].inode_cache) != FS_SUCCESS) {
        return FS_ERROR_INVALID;
    }
    
    // Initialize file descriptor
    file_descriptors[fd].inode_num = inode_num;
    file_descriptors[fd].position = 0;
    file_descriptors[fd].flags = flags;
    file_descriptors[fd].in_use = true;
    
    return fd;
}

int fs_close(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !file_descriptors[fd].in_use) {
        return FS_ERROR_INVALID;
    }
    
    file_descriptors[fd].in_use = false;
    return FS_SUCCESS;
}

int fs_read(int fd, void* buffer, uint32_t size) {
    (void)size; // TODO: Implement actual file reading
    
    if (fd < 0 || fd >= MAX_OPEN_FILES || !file_descriptors[fd].in_use || !buffer) {
        return FS_ERROR_INVALID;
    }
    
    // TODO: Implement actual file reading
    // For now, return 0 (no data read)
    return 0;
}

int fs_write(int fd, const void* buffer, uint32_t size) {
    (void)size; // TODO: Implement actual file writing
    
    if (fd < 0 || fd >= MAX_OPEN_FILES || !file_descriptors[fd].in_use || !buffer) {
        return FS_ERROR_INVALID;
    }
    
    // TODO: Implement actual file writing
    // For now, return 0 (no data written)
    return 0;
}

int fs_seek(int fd, uint32_t position) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !file_descriptors[fd].in_use) {
        return FS_ERROR_INVALID;
    }
    
    if (position > file_descriptors[fd].inode_cache.size) {
        return FS_ERROR_INVALID;
    }
    
    file_descriptors[fd].position = position;
    return FS_SUCCESS;
}