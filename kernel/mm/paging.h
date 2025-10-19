#ifndef PAGING_H
#define PAGING_H

#include "../types.h"

// Page size and related constants
#define PAGE_SIZE           4096
#define PAGE_ENTRIES        1024
#define PAGE_DIRECTORY_SIZE 1024

// Virtual memory layout (32-bit addresses)
#define KERNEL_VIRTUAL_BASE     0xC0000000  // 3GB - kernel space starts here
#define USER_VIRTUAL_BASE       0x00400000  // 4MB - user space starts here
#define USER_VIRTUAL_END        0xBFFFFFFF  // End of user space
#define KERNEL_HEAP_VIRTUAL     0xC0400000  // Kernel heap in virtual space

// Page permissions
#define PAGE_PRESENT    0x001  // Page is present in memory
#define PAGE_WRITABLE   0x002  // Page is writable
#define PAGE_USER       0x004  // Page is accessible from user space
#define PAGE_WRITE_THROUGH 0x008  // Write-through caching
#define PAGE_CACHE_DISABLE 0x010  // Caching disabled
#define PAGE_ACCESSED   0x020  // Page has been accessed
#define PAGE_DIRTY      0x040  // Page has been written to
#define PAGE_SIZE_4MB   0x080  // 4MB page (when PSE is enabled)
#define PAGE_GLOBAL     0x100  // Global page (when PGE is enabled)

// Page directory and page table entry structures
typedef struct page_directory_entry {
    uint32_t present        : 1;   // Page present in memory
    uint32_t writable       : 1;   // Read/write permission
    uint32_t user           : 1;   // User/supervisor mode
    uint32_t write_through  : 1;   // Write-through caching
    uint32_t cache_disable  : 1;   // Cache disabled
    uint32_t accessed       : 1;   // Accessed flag
    uint32_t reserved       : 1;   // Reserved (must be 0)
    uint32_t page_size      : 1;   // Page size (0 = 4KB, 1 = 4MB)
    uint32_t global         : 1;   // Global page
    uint32_t available      : 3;   // Available for OS use
    uint32_t page_table     : 20;  // Physical address of page table >> 12
} __attribute__((packed)) page_directory_entry_t;

typedef struct page_table_entry {
    uint32_t present        : 1;   // Page present in memory
    uint32_t writable       : 1;   // Read/write permission
    uint32_t user           : 1;   // User/supervisor mode
    uint32_t write_through  : 1;   // Write-through caching
    uint32_t cache_disable  : 1;   // Cache disabled
    uint32_t accessed       : 1;   // Accessed flag
    uint32_t dirty          : 1;   // Dirty flag
    uint32_t reserved       : 1;   // Reserved (must be 0)
    uint32_t global         : 1;   // Global page
    uint32_t available      : 3;   // Available for OS use
    uint32_t page_frame     : 20;  // Physical address of page >> 12
} __attribute__((packed)) page_table_entry_t;

// Page directory and page table structures
typedef struct page_directory {
    page_directory_entry_t entries[PAGE_DIRECTORY_SIZE];
} __attribute__((aligned(PAGE_SIZE))) page_directory_t;

typedef struct page_table {
    page_table_entry_t entries[PAGE_ENTRIES];
} __attribute__((aligned(PAGE_SIZE))) page_table_t;

// Physical page frame allocator
typedef struct page_frame {
    uint32_t physical_addr;
    uint32_t ref_count;
    struct page_frame* next;
} page_frame_t;

// Memory statistics
typedef struct memory_stats {
    uint32_t total_pages;
    uint32_t free_pages;
    uint32_t used_pages;
    uint32_t kernel_pages;
    uint32_t user_pages;
    uint32_t page_faults;
    uint32_t page_fault_resolved;
} memory_stats_t;

// Function prototypes
void paging_init(void);
void enable_paging(void);
void switch_page_directory(page_directory_t* dir);

// Page directory management
page_directory_t* create_page_directory(void);
void destroy_page_directory(page_directory_t* dir);

// Virtual memory mapping
int map_page(page_directory_t* dir, uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags);
int unmap_page(page_directory_t* dir, uint32_t virtual_addr);
uint32_t get_physical_address(page_directory_t* dir, uint32_t virtual_addr);

// Page frame allocation
uint32_t allocate_page_frame(void);
void free_page_frame(uint32_t physical_addr);

// Memory protection
int set_page_permissions(page_directory_t* dir, uint32_t virtual_addr, uint32_t flags);
int is_page_accessible(page_directory_t* dir, uint32_t virtual_addr, uint32_t required_flags);

// Page fault handling
void page_fault_handler(uint32_t error_code, uint32_t virtual_addr);

// Memory statistics and debugging
void get_memory_stats(memory_stats_t* stats);
void print_memory_stats(void);
void print_page_directory(page_directory_t* dir);

// Utility functions
uint32_t virtual_to_page_index(uint32_t virtual_addr);
uint32_t virtual_to_table_index(uint32_t virtual_addr);
uint32_t page_align_down(uint32_t addr);
uint32_t page_align_up(uint32_t addr);

// Global variables
extern page_directory_t* kernel_page_directory;
extern page_directory_t* current_page_directory;

#endif // PAGING_H