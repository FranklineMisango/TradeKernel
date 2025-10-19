#ifndef MEMORY_H
#define MEMORY_H

#include "../types.h"

// Memory layout constants
#define KERNEL_START      0x10000   // 64KB - where kernel is loaded
#define KERNEL_HEAP_START 0x100000  // 1MB - start of kernel heap
#define KERNEL_HEAP_SIZE  0x400000  // 4MB - size of kernel heap

// Page size
#define PAGE_SIZE 4096

// Memory debugging constants
#define MEMORY_GUARD_MAGIC    0xDEADBEEF
#define MEMORY_FREE_MAGIC     0xFEEDFACE
#define MAX_ALLOCATIONS       1024

// Memory block structure for enhanced heap allocator
typedef struct mem_block {
    uint32_t magic;         // Guard magic number
    size_t size;            // Size of the block
    int free;               // Free flag
    const char* file;       // File where allocated (for debugging)
    int line;               // Line number where allocated
    uint32_t alloc_id;      // Unique allocation ID
    struct mem_block* next; // Next block
    struct mem_block* prev; // Previous block (for faster coalescing)
} mem_block_t;

// Allocation tracking structure
typedef struct allocation_info {
    void* ptr;
    size_t size;
    const char* file;
    int line;
    uint32_t alloc_id;
    uint32_t timestamp;
} allocation_info_t;

// Memory statistics structure
typedef struct heap_stats {
    size_t total_memory;
    size_t used_memory;
    size_t free_memory;
    uint32_t total_allocations;
    uint32_t active_allocations;
    uint32_t failed_allocations;
    uint32_t free_operations;
    uint32_t coalesce_operations;
    size_t largest_free_block;
    size_t fragmentation_ratio;
} heap_stats_t;

// Memory debugging macros
#ifdef MEMORY_DEBUG
    #define kmalloc(size) _kmalloc_debug(size, __FILE__, __LINE__)
    #define kcalloc(count, size) _kcalloc_debug(count, size, __FILE__, __LINE__)
    #define krealloc(ptr, size) _krealloc_debug(ptr, size, __FILE__, __LINE__)
    #define kfree(ptr) _kfree_debug(ptr, __FILE__, __LINE__)
#else
    #define kcalloc(count, size) _kcalloc(count, size)
    #define krealloc(ptr, size) _krealloc(ptr, size)
#endif

// Function prototypes
void memory_init(void);

// Basic allocation functions
void* kmalloc(size_t size);
void* kcalloc(size_t count, size_t size);
void* krealloc(void* ptr, size_t size);
void kfree(void* ptr);

// Debug allocation functions
void* _kmalloc_debug(size_t size, const char* file, int line);
void* _kcalloc_debug(size_t count, size_t size, const char* file, int line);
void* _krealloc_debug(void* ptr, size_t size, const char* file, int line);
void _kfree_debug(void* ptr, const char* file, int line);

// Non-debug allocation functions
void* _kcalloc(size_t count, size_t size);
void* _krealloc(void* ptr, size_t size);

// Memory statistics and debugging
size_t get_free_memory(void);
size_t get_total_memory(void);
void get_heap_stats(heap_stats_t* stats);
void print_heap_stats(void);
void print_allocation_list(void);
int check_heap_integrity(void);
void detect_memory_leaks(void);
void print_number(uint32_t num);

// Memory utility functions
void* memset(void* dest, int val, size_t count);
void* memcpy(void* dest, const void* src, size_t count);
int memcmp(const void* ptr1, const void* ptr2, size_t count);
size_t strlen(const char* str);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
int strcmp(const char* str1, const char* str2);

// Memory pool allocator for fixed-size blocks
typedef struct memory_pool {
    void* pool_start;
    size_t block_size;
    size_t block_count;
    uint32_t* free_bitmap;
    uint32_t free_blocks;
} memory_pool_t;

memory_pool_t* create_memory_pool(size_t block_size, size_t block_count);
void* pool_alloc(memory_pool_t* pool);
void pool_free(memory_pool_t* pool, void* ptr);
void destroy_memory_pool(memory_pool_t* pool);

#endif // MEMORY_H