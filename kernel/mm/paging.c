#include "paging.h"
#include "memory.h"
#include "../drivers/vga.h"

// Global page directory and current directory
page_directory_t* kernel_page_directory = NULL;
page_directory_t* current_page_directory = NULL;

// Physical page frame allocator
static page_frame_t* free_page_frames = NULL;
static uint32_t next_page_frame = 0x200000; // Start allocating after 2MB
static uint32_t max_memory = 0x1000000;     // Assume 16MB for now

// Memory statistics
static memory_stats_t mem_stats = {0};

// External assembly functions (defined in paging.asm)
extern void load_page_directory(uint32_t physical_addr);
extern void enable_paging_asm(void);
extern uint32_t get_page_fault_address(void);

// Initialize the paging system
void paging_init(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("Initializing paging system...\n");
    
    // Calculate total pages available
    mem_stats.total_pages = max_memory / PAGE_SIZE;
    mem_stats.free_pages = mem_stats.total_pages - (next_page_frame / PAGE_SIZE);
    mem_stats.used_pages = 0;
    mem_stats.kernel_pages = 0;
    mem_stats.user_pages = 0;
    mem_stats.page_faults = 0;
    mem_stats.page_fault_resolved = 0;
    
    // For now, just initialize basic paging structures without enabling paging
    // This keeps the system stable while providing the memory management framework
    kernel_page_directory = NULL;  // Will be set up later when paging is actually needed
    current_page_directory = NULL;
    
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write_string("Paging system initialized (disabled for stability)\n");
}

// Enable paging
void enable_paging(void) {
    if (!kernel_page_directory) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Cannot enable paging: no page directory!\n");
        return;
    }
    
    // Load the page directory
    uint32_t page_dir_phys = (uint32_t)kernel_page_directory;
    load_page_directory(page_dir_phys);
    
    // Enable paging in CR0
    enable_paging_asm();
    
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write_string("Paging enabled successfully\n");
}

// Switch to a different page directory
void switch_page_directory(page_directory_t* dir) {
    if (!dir) return;
    
    current_page_directory = dir;
    uint32_t page_dir_phys = (uint32_t)dir;
    load_page_directory(page_dir_phys);
}

// Create a new page directory
page_directory_t* create_page_directory(void) {
    // Allocate memory for page directory
    page_directory_t* dir = (page_directory_t*)kmalloc(sizeof(page_directory_t));
    if (!dir) return NULL;
    
    // Clear the page directory
    memset(dir, 0, sizeof(page_directory_t));
    
    return dir;
}

// Destroy a page directory
void destroy_page_directory(page_directory_t* dir) {
    if (!dir || dir == kernel_page_directory) return;
    
    // Free all page tables
    for (int i = 0; i < PAGE_DIRECTORY_SIZE; i++) {
        if (dir->entries[i].present) {
            uint32_t page_table_phys = dir->entries[i].page_table << 12;
            page_table_t* page_table = (page_table_t*)page_table_phys;
            
            // Free all pages in this table
            for (int j = 0; j < PAGE_ENTRIES; j++) {
                if (page_table->entries[j].present) {
                    uint32_t page_phys = page_table->entries[j].page_frame << 12;
                    free_page_frame(page_phys);
                }
            }
            
            // Free the page table itself
            kfree(page_table);
        }
    }
    
    // Free the page directory
    kfree(dir);
}

// Map a virtual address to a physical address
int map_page(page_directory_t* dir, uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags) {
    if (!dir) return -1;
    
    uint32_t page_dir_index = virtual_to_page_index(virtual_addr);
    uint32_t page_table_index = virtual_to_table_index(virtual_addr);
    
    // Get or create page table
    page_table_t* page_table;
    if (!dir->entries[page_dir_index].present) {
        // Allocate new page table
        page_table = (page_table_t*)kmalloc(sizeof(page_table_t));
        if (!page_table) return -1;
        
        memset(page_table, 0, sizeof(page_table_t));
        
        // Set up page directory entry
        dir->entries[page_dir_index].present = 1;
        dir->entries[page_dir_index].writable = (flags & PAGE_WRITABLE) ? 1 : 0;
        dir->entries[page_dir_index].user = (flags & PAGE_USER) ? 1 : 0;
        dir->entries[page_dir_index].page_table = ((uint32_t)page_table) >> 12;
    } else {
        page_table = (page_table_t*)(dir->entries[page_dir_index].page_table << 12);
    }
    
    // Set up page table entry
    page_table->entries[page_table_index].present = (flags & PAGE_PRESENT) ? 1 : 0;
    page_table->entries[page_table_index].writable = (flags & PAGE_WRITABLE) ? 1 : 0;
    page_table->entries[page_table_index].user = (flags & PAGE_USER) ? 1 : 0;
    page_table->entries[page_table_index].page_frame = physical_addr >> 12;
    
    // Update statistics
    mem_stats.used_pages++;
    if (flags & PAGE_USER) {
        mem_stats.user_pages++;
    } else {
        mem_stats.kernel_pages++;
    }
    
    return 0;
}

// Unmap a virtual address
int unmap_page(page_directory_t* dir, uint32_t virtual_addr) {
    if (!dir) return -1;
    
    uint32_t page_dir_index = virtual_to_page_index(virtual_addr);
    uint32_t page_table_index = virtual_to_table_index(virtual_addr);
    
    if (!dir->entries[page_dir_index].present) {
        return -1; // Page table doesn't exist
    }
    
    page_table_t* page_table = (page_table_t*)(dir->entries[page_dir_index].page_table << 12);
    
    if (!page_table->entries[page_table_index].present) {
        return -1; // Page not mapped
    }
    
    // Free the physical page
    uint32_t physical_addr = page_table->entries[page_table_index].page_frame << 12;
    free_page_frame(physical_addr);
    
    // Clear page table entry
    page_table->entries[page_table_index].present = 0;
    page_table->entries[page_table_index].page_frame = 0;
    
    // Update statistics
    mem_stats.used_pages--;
    
    // Invalidate TLB entry
    __asm__ volatile ("invlpg (%0)" : : "r" (virtual_addr) : "memory");
    
    return 0;
}

// Get physical address for a virtual address
uint32_t get_physical_address(page_directory_t* dir, uint32_t virtual_addr) {
    if (!dir) return 0;
    
    uint32_t page_dir_index = virtual_to_page_index(virtual_addr);
    uint32_t page_table_index = virtual_to_table_index(virtual_addr);
    uint32_t page_offset = virtual_addr & 0xFFF;
    
    if (!dir->entries[page_dir_index].present) {
        return 0; // Page table doesn't exist
    }
    
    page_table_t* page_table = (page_table_t*)(dir->entries[page_dir_index].page_table << 12);
    
    if (!page_table->entries[page_table_index].present) {
        return 0; // Page not mapped
    }
    
    return (page_table->entries[page_table_index].page_frame << 12) | page_offset;
}

// Allocate a physical page frame
uint32_t allocate_page_frame(void) {
    if (free_page_frames) {
        // Use a previously freed page
        page_frame_t* frame = free_page_frames;
        free_page_frames = frame->next;
        uint32_t addr = frame->physical_addr;
        kfree(frame);
        mem_stats.free_pages--;
        return addr;
    }
    
    // Allocate a new page frame
    if (next_page_frame + PAGE_SIZE > max_memory) {
        return 0; // Out of memory
    }
    
    uint32_t addr = next_page_frame;
    next_page_frame += PAGE_SIZE;
    mem_stats.free_pages--;
    return addr;
}

// Free a physical page frame
void free_page_frame(uint32_t physical_addr) {
    page_frame_t* frame = (page_frame_t*)kmalloc(sizeof(page_frame_t));
    if (!frame) return;
    
    frame->physical_addr = physical_addr;
    frame->ref_count = 0;
    frame->next = free_page_frames;
    free_page_frames = frame;
    mem_stats.free_pages++;
}

// Set page permissions
int set_page_permissions(page_directory_t* dir, uint32_t virtual_addr, uint32_t flags) {
    if (!dir) return -1;
    
    uint32_t page_dir_index = virtual_to_page_index(virtual_addr);
    uint32_t page_table_index = virtual_to_table_index(virtual_addr);
    
    if (!dir->entries[page_dir_index].present) {
        return -1; // Page table doesn't exist
    }
    
    page_table_t* page_table = (page_table_t*)(dir->entries[page_dir_index].page_table << 12);
    
    if (!page_table->entries[page_table_index].present) {
        return -1; // Page not mapped
    }
    
    // Update permissions
    page_table->entries[page_table_index].writable = (flags & PAGE_WRITABLE) ? 1 : 0;
    page_table->entries[page_table_index].user = (flags & PAGE_USER) ? 1 : 0;
    
    // Invalidate TLB entry
    __asm__ volatile ("invlpg (%0)" : : "r" (virtual_addr) : "memory");
    
    return 0;
}

// Check if page is accessible with required flags
int is_page_accessible(page_directory_t* dir, uint32_t virtual_addr, uint32_t required_flags) {
    if (!dir) return 0;
    
    uint32_t page_dir_index = virtual_to_page_index(virtual_addr);
    uint32_t page_table_index = virtual_to_table_index(virtual_addr);
    
    if (!dir->entries[page_dir_index].present) {
        return 0; // Page table doesn't exist
    }
    
    page_table_t* page_table = (page_table_t*)(dir->entries[page_dir_index].page_table << 12);
    
    if (!page_table->entries[page_table_index].present) {
        return 0; // Page not mapped
    }
    
    // Check permissions
    if ((required_flags & PAGE_WRITABLE) && !page_table->entries[page_table_index].writable) {
        return 0;
    }
    
    if ((required_flags & PAGE_USER) && !page_table->entries[page_table_index].user) {
        return 0;
    }
    
    return 1;
}

// Page fault handler
void page_fault_handler(uint32_t error_code, uint32_t virtual_addr) {
    mem_stats.page_faults++;
    
    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_write_string("Page fault at address: 0x");
    
    // Print address in hex
    char hex_str[9];
    for (int i = 7; i >= 0; i--) {
        int digit = (virtual_addr >> (i * 4)) & 0xF;
        hex_str[7-i] = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
    }
    hex_str[8] = '\0';
    vga_write_string(hex_str);
    
    vga_write_string("\nError code: 0x");
    for (int i = 7; i >= 0; i--) {
        int digit = (error_code >> (i * 4)) & 0xF;
        hex_str[7-i] = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
    }
    vga_write_string(hex_str);
    vga_write_string("\n");
    
    if (error_code & 0x1) {
        vga_write_string("Page protection violation\n");
    } else {
        vga_write_string("Page not present\n");
    }
    
    if (error_code & 0x2) {
        vga_write_string("Write access\n");
    } else {
        vga_write_string("Read access\n");
    }
    
    if (error_code & 0x4) {
        vga_write_string("User mode access\n");
    } else {
        vga_write_string("Kernel mode access\n");
    }
    
    // For now, halt the system on page fault
    vga_write_string("System halted due to page fault\n");
    __asm__ volatile ("hlt");
}

// Get memory statistics
void get_memory_stats(memory_stats_t* stats) {
    if (stats) {
        *stats = mem_stats;
    }
}

// Print memory statistics
void print_memory_stats(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("=== Virtual Memory Statistics ===\n");
    
    if (kernel_page_directory == NULL) {
        vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
        vga_write_string("Virtual memory not active (running in identity mapping mode)\n");
        vga_write_string("Paging framework initialized but disabled for stability\n");
        return;
    }
    
    vga_write_string("Total pages: ");
    
    // Convert to string and print (simple implementation)
    char num_str[12];
    uint32_t num = mem_stats.total_pages;
    int pos = 0;
    do {
        num_str[pos++] = '0' + (num % 10);
        num /= 10;
    } while (num > 0);
    
    // Reverse string
    for (int i = 0; i < pos / 2; i++) {
        char temp = num_str[i];
        num_str[i] = num_str[pos - 1 - i];
        num_str[pos - 1 - i] = temp;
    }
    num_str[pos] = '\0';
    
    vga_write_string(num_str);
    vga_write_string("\n");
}

// Utility functions
uint32_t virtual_to_page_index(uint32_t virtual_addr) {
    return (virtual_addr >> 22) & 0x3FF;
}

uint32_t virtual_to_table_index(uint32_t virtual_addr) {
    return (virtual_addr >> 12) & 0x3FF;
}

uint32_t page_align_down(uint32_t addr) {
    return addr & ~(PAGE_SIZE - 1);
}

uint32_t page_align_up(uint32_t addr) {
    return (addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}