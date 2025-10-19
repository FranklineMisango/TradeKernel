#include "memory.h"
#include "../drivers/vga.h"

static mem_block_t* heap_start = NULL;
static heap_stats_t heap_stats = {0};
static allocation_info_t allocations[MAX_ALLOCATIONS];
static uint32_t next_alloc_id = 1;

void memory_init(void) {
    heap_start = (mem_block_t*)KERNEL_HEAP_START;
    heap_start->magic = MEMORY_GUARD_MAGIC;
    heap_start->size = KERNEL_HEAP_SIZE - sizeof(mem_block_t);
    heap_start->free = 1;
    heap_start->file = "system";
    heap_start->line = 0;
    heap_start->alloc_id = 0;
    heap_start->next = NULL;
    heap_start->prev = NULL;
    
    // Initialize heap statistics
    heap_stats.total_memory = KERNEL_HEAP_SIZE;
    heap_stats.used_memory = sizeof(mem_block_t);
    heap_stats.free_memory = KERNEL_HEAP_SIZE - sizeof(mem_block_t);
    heap_stats.total_allocations = 0;
    heap_stats.active_allocations = 0;
    heap_stats.failed_allocations = 0;
    heap_stats.free_operations = 0;
    heap_stats.coalesce_operations = 0;
    heap_stats.largest_free_block = heap_start->size;
    heap_stats.fragmentation_ratio = 0;
    
    // Clear allocation tracking
    memset(allocations, 0, sizeof(allocations));
}

void* kmalloc(size_t size) {
    return _kmalloc_debug(size, "unknown", 0);
}

void* _kmalloc_debug(size_t size, const char* file, int line) {
    if (size == 0) return NULL;
    
    // Align size to 8-byte boundary
    size = (size + 7) & ~7;
    
    mem_block_t* current = heap_start;
    mem_block_t* best_fit = NULL;
    
    // Best-fit allocation algorithm
    while (current != NULL) {
        // Check magic number for corruption detection
        if (current->magic != MEMORY_GUARD_MAGIC) {
            vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            vga_write_string("Heap corruption detected in kmalloc!\n");
            return NULL;
        }
        
        if (current->free && current->size >= size) {
            if (best_fit == NULL || current->size < best_fit->size) {
                best_fit = current;
            }
        }
        current = current->next;
    }
    
    if (best_fit == NULL) {
        heap_stats.failed_allocations++;
        return NULL; // Out of memory
    }
    
    // Split block if it's much larger than needed
    if (best_fit->size > size + sizeof(mem_block_t) + 16) {
        mem_block_t* new_block = (mem_block_t*)((char*)best_fit + sizeof(mem_block_t) + size);
        new_block->magic = MEMORY_GUARD_MAGIC;
        new_block->size = best_fit->size - size - sizeof(mem_block_t);
        new_block->free = 1;
        new_block->file = "split";
        new_block->line = 0;
        new_block->alloc_id = 0;
        new_block->next = best_fit->next;
        new_block->prev = best_fit;
        
        if (best_fit->next) {
            best_fit->next->prev = new_block;
        }
        
        best_fit->size = size;
        best_fit->next = new_block;
    }
    
    best_fit->free = 0;
    best_fit->file = file;
    best_fit->line = line;
    best_fit->alloc_id = next_alloc_id++;
    
    // Update statistics
    heap_stats.used_memory += best_fit->size;
    heap_stats.free_memory -= best_fit->size;
    heap_stats.total_allocations++;
    heap_stats.active_allocations++;
    
    // Track allocation
    for (int i = 0; i < MAX_ALLOCATIONS; i++) {
        if (allocations[i].ptr == NULL) {
            allocations[i].ptr = (char*)best_fit + sizeof(mem_block_t);
            allocations[i].size = best_fit->size;
            allocations[i].file = file;
            allocations[i].line = line;
            allocations[i].alloc_id = best_fit->alloc_id;
            allocations[i].timestamp = heap_stats.total_allocations;
            break;
        }
    }
    
    return (char*)best_fit + sizeof(mem_block_t);
}

void kfree(void* ptr) {
    _kfree_debug(ptr, "unknown", 0);
}

void _kfree_debug(void* ptr, const char* file, int line) {
    (void)file; (void)line; // Suppress unused parameter warnings
    if (ptr == NULL) return;
    
    mem_block_t* block = (mem_block_t*)((char*)ptr - sizeof(mem_block_t));
    
    // Check magic number for corruption detection
    if (block->magic != MEMORY_GUARD_MAGIC) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Double free or corruption detected in kfree!\n");
        return;
    }
    
    if (block->free) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Double free detected!\n");
        return;
    }
    
    block->free = 1;
    block->magic = MEMORY_FREE_MAGIC;  // Mark as freed
    
    // Update statistics
    heap_stats.used_memory -= block->size;
    heap_stats.free_memory += block->size;
    heap_stats.free_operations++;
    heap_stats.active_allocations--;
    
    // Remove from allocation tracking
    for (int i = 0; i < MAX_ALLOCATIONS; i++) {
        if (allocations[i].ptr == ptr) {
            allocations[i].ptr = NULL;
            break;
        }
    }
    
    // Coalesce with next block if it's free
    if (block->next && block->next->free) {
        block->size += block->next->size + sizeof(mem_block_t);
        if (block->next->next) {
            block->next->next->prev = block;
        }
        block->next = block->next->next;
        heap_stats.coalesce_operations++;
    }
    
    // Coalesce with previous block if it's free
    if (block->prev && block->prev->free) {
        block->prev->size += block->size + sizeof(mem_block_t);
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
        heap_stats.coalesce_operations++;
    }
    
    // Clear the memory content for security
    memset(ptr, 0xDD, block->size);
}

// Enhanced calloc implementation
void* _kcalloc_debug(size_t count, size_t size, const char* file, int line) {
    size_t total_size = count * size;
    if (total_size / count != size) return NULL; // Overflow check
    
    void* ptr = _kmalloc_debug(total_size, file, line);
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    return ptr;
}

void* _kcalloc(size_t count, size_t size) {
    return _kcalloc_debug(count, size, "unknown", 0);
}

// Enhanced realloc implementation
void* _krealloc_debug(void* ptr, size_t size, const char* file, int line) {
    if (ptr == NULL) {
        return _kmalloc_debug(size, file, line);
    }
    
    if (size == 0) {
        _kfree_debug(ptr, file, line);
        return NULL;
    }
    
    mem_block_t* block = (mem_block_t*)((char*)ptr - sizeof(mem_block_t));
    if (block->magic != MEMORY_GUARD_MAGIC) {
        return NULL; // Corrupted block
    }
    
    if (block->size >= size) {
        return ptr; // Current block is large enough
    }
    
    // Allocate new block and copy data
    void* new_ptr = _kmalloc_debug(size, file, line);
    if (new_ptr) {
        memcpy(new_ptr, ptr, block->size < size ? block->size : size);
        _kfree_debug(ptr, file, line);
    }
    
    return new_ptr;
}

void* _krealloc(void* ptr, size_t size) {
    return _krealloc_debug(ptr, size, "unknown", 0);
}

size_t get_free_memory(void) {
    return heap_stats.free_memory;
}

size_t get_total_memory(void) {
    return heap_stats.total_memory;
}

void get_heap_stats(heap_stats_t* stats) {
    if (stats) {
        // Update largest free block
        heap_stats.largest_free_block = 0;
        mem_block_t* current = heap_start;
        while (current) {
            if (current->free && current->size > heap_stats.largest_free_block) {
                heap_stats.largest_free_block = current->size;
            }
            current = current->next;
        }
        
        // Calculate fragmentation ratio
        if (heap_stats.free_memory > 0) {
            heap_stats.fragmentation_ratio = 
                (heap_stats.largest_free_block * 100) / heap_stats.free_memory;
        }
        
        *stats = heap_stats;
    }
}

void print_heap_stats(void) {
    heap_stats_t stats;
    get_heap_stats(&stats);
    
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("=== Heap Statistics ===\n");
    vga_write_string("Total Memory: ");
    print_number(stats.total_memory);
    vga_write_string(" bytes\n");
    
    vga_write_string("Used Memory: ");
    print_number(stats.used_memory);
    vga_write_string(" bytes\n");
    
    vga_write_string("Free Memory: ");
    print_number(stats.free_memory);
    vga_write_string(" bytes\n");
    
    vga_write_string("Active Allocations: ");
    print_number(stats.active_allocations);
    vga_write_string("\n");
    
    vga_write_string("Failed Allocations: ");
    print_number(stats.failed_allocations);
    vga_write_string("\n");
    
    vga_write_string("Largest Free Block: ");
    print_number(stats.largest_free_block);
    vga_write_string(" bytes\n");
}

void print_allocation_list(void) {
    vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    vga_write_string("=== Active Allocations ===\n");
    
    for (int i = 0; i < MAX_ALLOCATIONS; i++) {
        if (allocations[i].ptr != NULL) {
            vga_write_string("ID: ");
            print_number(allocations[i].alloc_id);
            vga_write_string(" Size: ");
            print_number(allocations[i].size);
            vga_write_string(" File: ");
            vga_write_string(allocations[i].file ? allocations[i].file : "unknown");
            vga_write_string("\n");
        }
    }
}

int check_heap_integrity(void) {
    mem_block_t* current = heap_start;
    int errors = 0;
    
    while (current) {
        if (current->free && current->magic != MEMORY_GUARD_MAGIC) {
            if (current->magic != MEMORY_FREE_MAGIC) {
                vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
                vga_write_string("Heap corruption detected!\n");
                errors++;
            }
        } else if (!current->free && current->magic != MEMORY_GUARD_MAGIC) {
            vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            vga_write_string("Allocated block corruption detected!\n");
            errors++;
        }
        current = current->next;
    }
    
    return errors;
}

void detect_memory_leaks(void) {
    int leaks = 0;
    vga_set_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
    vga_write_string("=== Memory Leak Detection ===\n");
    
    for (int i = 0; i < MAX_ALLOCATIONS; i++) {
        if (allocations[i].ptr != NULL) {
            vga_write_string("LEAK: ");
            vga_write_string(allocations[i].file ? allocations[i].file : "unknown");
            vga_write_string(":");
            print_number(allocations[i].line);
            vga_write_string(" (");
            print_number(allocations[i].size);
            vga_write_string(" bytes)\n");
            leaks++;
        }
    }
    
    if (leaks == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_write_string("No memory leaks detected.\n");
    } else {
        vga_write_string("Total leaks: ");
        print_number(leaks);
        vga_write_string("\n");
    }
}

// Helper function to print numbers
void print_number(uint32_t num) {
    char buffer[12];
    int pos = 0;
    
    if (num == 0) {
        vga_putchar('0');
        return;
    }
    
    while (num > 0) {
        buffer[pos++] = '0' + (num % 10);
        num /= 10;
    }
    
    for (int i = pos - 1; i >= 0; i--) {
        vga_putchar(buffer[i]);
    }
}

// Memory utility functions
void* memset(void* dest, int val, size_t count) {
    unsigned char* ptr = (unsigned char*)dest;
    while (count--) {
        *ptr++ = (unsigned char)val;
    }
    return dest;
}

void* memcpy(void* dest, const void* src, size_t count) {
    unsigned char* dst = (unsigned char*)dest;
    const unsigned char* source = (const unsigned char*)src;
    while (count--) {
        *dst++ = *source++;
    }
    return dest;
}

int memcmp(const void* ptr1, const void* ptr2, size_t count) {
    const unsigned char* p1 = (const unsigned char*)ptr1;
    const unsigned char* p2 = (const unsigned char*)ptr2;
    while (count--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

char* strcpy(char* dest, const char* src) {
    char* orig_dest = dest;
    while ((*dest++ = *src++));
    return orig_dest;
}

char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

int strcmp(const char* str1, const char* str2) {
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return *(unsigned char*)str1 - *(unsigned char*)str2;
}

// Memory pool allocator implementation
memory_pool_t* create_memory_pool(size_t block_size, size_t block_count) {
    memory_pool_t* pool = (memory_pool_t*)kmalloc(sizeof(memory_pool_t));
    if (!pool) return NULL;
    
    // Align block size to 8-byte boundary
    block_size = (block_size + 7) & ~7;
    
    // Allocate pool memory
    pool->pool_start = kmalloc(block_size * block_count);
    if (!pool->pool_start) {
        kfree(pool);
        return NULL;
    }
    
    pool->block_size = block_size;
    pool->block_count = block_count;
    pool->free_blocks = block_count;
    
    // Allocate and initialize bitmap
    size_t bitmap_size = (block_count + 31) / 32; // Round up to 32-bit boundaries
    pool->free_bitmap = (uint32_t*)kmalloc(bitmap_size * sizeof(uint32_t));
    if (!pool->free_bitmap) {
        kfree(pool->pool_start);
        kfree(pool);
        return NULL;
    }
    
    // Initialize all blocks as free
    memset(pool->free_bitmap, 0xFF, bitmap_size * sizeof(uint32_t));
    
    return pool;
}

void* pool_alloc(memory_pool_t* pool) {
    if (!pool || pool->free_blocks == 0) return NULL;
    
    // Find first free block
    for (size_t i = 0; i < pool->block_count; i++) {
        uint32_t word_index = i / 32;
        uint32_t bit_index = i % 32;
        
        if (pool->free_bitmap[word_index] & (1 << bit_index)) {
            // Mark block as used
            pool->free_bitmap[word_index] &= ~(1 << bit_index);
            pool->free_blocks--;
            
            // Return pointer to block
            return (char*)pool->pool_start + (i * pool->block_size);
        }
    }
    
    return NULL; // Should not happen if free_blocks > 0
}

void pool_free(memory_pool_t* pool, void* ptr) {
    if (!pool || !ptr) return;
    
    // Calculate block index
    size_t offset = (char*)ptr - (char*)pool->pool_start;
    if (offset % pool->block_size != 0) return; // Invalid pointer
    
    size_t block_index = offset / pool->block_size;
    if (block_index >= pool->block_count) return; // Out of range
    
    uint32_t word_index = block_index / 32;
    uint32_t bit_index = block_index % 32;
    
    // Mark block as free
    pool->free_bitmap[word_index] |= (1 << bit_index);
    pool->free_blocks++;
    
    // Clear block content for security
    memset(ptr, 0, pool->block_size);
}

void destroy_memory_pool(memory_pool_t* pool) {
    if (!pool) return;
    
    kfree(pool->free_bitmap);
    kfree(pool->pool_start);
    kfree(pool);
}