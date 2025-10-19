#ifndef FS_H
#define FS_H

#include "../types.h"

// File system constants
#define BLOCK_SIZE           512     // Size of each block in bytes
#define MAX_FILENAME_LENGTH  32      // Maximum filename length
#define MAX_PATH_LENGTH      256     // Maximum path length
#define MAX_OPEN_FILES       32      // Maximum number of open files
#define ROOT_INODE          1        // Root directory inode number

// File types
#define FILE_TYPE_REGULAR   0x01
#define FILE_TYPE_DIRECTORY 0x02

// File permissions (simple)
#define PERM_READ    0x04
#define PERM_WRITE   0x02
#define PERM_EXECUTE 0x01

// File system errors
#define FS_SUCCESS          0
#define FS_ERROR_NOT_FOUND  -1
#define FS_ERROR_NO_SPACE   -2
#define FS_ERROR_INVALID    -3
#define FS_ERROR_EXISTS     -4
#define FS_ERROR_NO_MEMORY  -5

// Superblock structure - describes the file system
typedef struct {
    uint32_t magic;           // Magic number to identify our filesystem
    uint32_t total_blocks;    // Total number of blocks
    uint32_t free_blocks;     // Number of free blocks
    uint32_t inode_blocks;    // Number of blocks used for inodes
    uint32_t data_blocks;     // Number of data blocks
    uint32_t block_size;      // Size of each block
    uint32_t inodes_per_block; // Number of inodes per block
    uint32_t total_inodes;    // Total number of inodes
    uint32_t free_inodes;     // Number of free inodes
    uint32_t root_inode;      // Root directory inode number
} __attribute__((packed)) superblock_t;

// Inode structure - describes a file or directory
typedef struct {
    uint32_t inode_num;       // Inode number
    uint8_t file_type;        // File type (regular file, directory, etc.)
    uint8_t permissions;      // File permissions
    uint16_t reserved;        // Reserved for alignment
    uint32_t size;            // File size in bytes
    uint32_t blocks_used;     // Number of blocks used by this file
    uint32_t created_time;    // Creation timestamp (placeholder)
    uint32_t modified_time;   // Last modification timestamp (placeholder)
    uint32_t direct_blocks[12]; // Direct block pointers
    uint32_t indirect_block;  // Single indirect block pointer
    uint32_t double_indirect; // Double indirect block pointer (for large files)
} __attribute__((packed)) inode_t;

// Directory entry structure
typedef struct {
    uint32_t inode_num;       // Inode number of the file/directory
    uint16_t name_length;     // Length of the filename
    uint8_t file_type;        // File type (for quick lookup)
    uint8_t reserved;         // Reserved for alignment
    char name[MAX_FILENAME_LENGTH]; // Filename (null-terminated)
} __attribute__((packed)) directory_entry_t;

// File descriptor structure for open files
typedef struct {
    uint32_t inode_num;       // Inode number
    uint32_t position;        // Current read/write position
    uint8_t flags;            // Open flags (read, write, etc.)
    uint8_t in_use;           // Whether this descriptor is in use
    inode_t inode_cache;      // Cached inode data
} file_descriptor_t;

// File system interface functions
int fs_init(void);
int fs_format(void);

// File operations
int fs_create_file(const char* path, uint8_t file_type);
int fs_delete_file(const char* path);
int fs_open(const char* path, uint8_t flags);
int fs_close(int fd);
int fs_read(int fd, void* buffer, uint32_t size);
int fs_write(int fd, const void* buffer, uint32_t size);
int fs_seek(int fd, uint32_t position);

// Directory operations
int fs_create_directory(const char* path);
int fs_list_directory(const char* path, directory_entry_t* entries, uint32_t max_entries);

// Utility functions
int fs_stat(const char* path, inode_t* stat_info);
int fs_exists(const char* path);
uint32_t fs_get_free_space(void);

// Internal functions (not exposed to user)
int _fs_read_superblock(superblock_t* sb);
int _fs_write_superblock(const superblock_t* sb);
int _fs_read_inode(uint32_t inode_num, inode_t* inode);
int _fs_write_inode(uint32_t inode_num, const inode_t* inode);
int _fs_allocate_block(void);
int _fs_free_block(uint32_t block_num);
int _fs_allocate_inode(void);
int _fs_free_inode(uint32_t inode_num);

#endif // FS_H