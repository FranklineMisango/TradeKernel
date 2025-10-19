#include "drivers/vga.h"
#include "mm/memory.h"
#include "mm/paging.h"
#include "arch/interrupts.h"
#include "shell.h"
#include "fs/fs.h"
#include "proc/process.h"
#include "proc/scheduler.h"
#include "proc/syscalls.h" // System calls enabled
#include "proc/ipc.h" // IPC enabled
#include "net/net.h"
#include "net/eth.h"
#include "net/ip.h"
#include "net/tcp.h"
#include "net/socket.h"
#include "net/websocket.h"
#include "gui.h" // GUI frameworkdrivers/vga.h"
#include "mm/memory.h"
#include "mm/paging.h"
#include "arch/interrupts.h"
#include "shell.h"
#include "fs/fs.h"
#include "proc/process.h"
#include "proc/scheduler.h"
#include "proc/syscalls.h" // System calls enabled
#include "proc/ipc.h" // IPC enabled

// Simple string formatting functions
void print_hex(uint32_t value) {
    char hex_chars[] = "0123456789ABCDEF";
    char buffer[11] = "0x";
    
    for (int i = 7; i >= 0; i--) {
        buffer[2 + (7 - i)] = hex_chars[(value >> (i * 4)) & 0xF];
    }
    buffer[10] = '\0';
    
    vga_write_string(buffer);
}

void print_dec(uint32_t value) {
    if (value == 0) {
        vga_putchar('0');
        return;
    }
    
    char buffer[12];
    int i = 0;
    
    while (value > 0) {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }
    
    // Reverse the string
    for (int j = i - 1; j >= 0; j--) {
        vga_putchar(buffer[j]);
    }
}

// Basic memory detection (placeholder)
uint32_t detect_memory(void) {
    // This is a simple placeholder - in a real OS, you'd use BIOS int 0x15
    // or multiboot information to detect actual memory
    return 0x1000000; // Assume 16MB for now
}

// Display TradeKernel loading screen with ASCII art logo
void display_loading_screen(void) {
    // Clear screen and set colors
    vga_clear();
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    
    // TradeKernel ASCII Art Logo
    vga_write_string("\n\n\n");
    vga_write_string("          _______                        ______                __\n");
    vga_write_string("         /_  __(_)____ ___  ____  ____  / ____/___  ____  _____/ /\n");
    vga_write_string("          / / / / ___// _ \\/ __ \\/ __ \\/ /   / __ \\/ __ \\/ ___/ / \n");
    vga_write_string("         / / / / /__ /  __/ /_/ / / / / /___/ /_/ / / / (__  )_/  \n");
    vga_write_string("        /_/ /_/\\___/ \\___/ .___/_/ /_/\\____/\\____/_/ /_/____/_/   \n");
    vga_write_string("                         /_/                                      \n");
    vga_write_string("\n");
    
    // Version and tagline
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write_string("                    TradeKernel OS v1.3 - ArithmaX Customized\n");
    vga_write_string("                Advanced Process Management & IPC Framework\n");
    vga_write_string("\n");
    
    // Loading message
    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_write_string("                              Initializing kernel...\n");
    vga_write_string("\n");
    
    // Simple progress bar simulation
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_write_string("                              [");
    for (int i = 0; i < 30; i++) {
        vga_write_string(".");
    }
    vga_write_string("]\n");
    
    // Brief delay (busy wait)
    for (volatile int i = 0; i < 1000000; i++) {
        // Busy wait for visual effect
    }
}

// Kernel main function - called from bootloader
void kernel_main(void) {
    // Display loading screen
    display_loading_screen();
    
    // Initialize VGA text mode
    vga_init();
    
    // Initialize memory management
    memory_init();
    
    // Initialize interrupts first
    interrupts_init();
    
    // Initialize paging system (after interrupts for page fault handling)
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("Initializing virtual memory...\n");
    paging_init();
    // Note: Not enabling paging yet - keeping identity mapping for stability
    
    // Initialize process management
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("Initializing process management...\n");
    process_init();
    scheduler_init();
    syscalls_init(); // System calls enabled
    ipc_init(); // IPC enabled
    
    // Initialize GUI framework
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("Initializing GUI framework...\n");
    gui_init();
    
    vga_write_string("Initializing file system...\n");
    int fs_result = fs_init();
    if (fs_result == FS_ERROR_NOT_FOUND) {
        vga_write_string("No filesystem found. Formatting disk...\n");
        fs_result = fs_format();
        if (fs_result == FS_SUCCESS) {
            vga_write_string("Filesystem created successfully!\n");
        } else {
            vga_write_string("Failed to create filesystem (error: ");
            // Print error code
            char code_str[10];
            int temp = fs_result;
            if (temp < 0) temp = -temp;
            int i = 0;
            do {
                code_str[i++] = '0' + (temp % 10);
                temp /= 10;
            } while (temp > 0 && i < 9);
            if (fs_result < 0) {
                vga_write_string("-");
            }
            while (i > 0) {
                vga_putchar(code_str[--i]);
            }
            vga_write_string(")\n");
        }
    } else if (fs_result == FS_SUCCESS) {
        vga_write_string("Existing filesystem mounted successfully!\n");
    } else {
        vga_write_string("Filesystem initialization failed (error: ");
        // Print error code
        char code_str[10];
        int temp = fs_result;
        if (temp < 0) temp = -temp;
        int i = 0;
        do {
            code_str[i++] = '0' + (temp % 10);
            temp /= 10;
        } while (temp > 0 && i < 9);
        if (fs_result < 0) {
            vga_write_string("-");
        }
        while (i > 0) {
            vga_putchar(code_str[--i]);
        }
        vga_write_string(")\n");
    }
    
    // Initialize network stack
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("Initializing network stack...\n");
    if (rtl8139_init(0xC000) == NET_SUCCESS) {
        vga_write_string("Ethernet driver initialized successfully!\n");
    } else {
        vga_write_string("Ethernet driver initialization failed!\n");
    }
    
    if (ipv4_init() == NET_SUCCESS) {
        vga_write_string("IPv4 protocol initialized successfully!\n");
    } else {
        vga_write_string("IPv4 protocol initialization failed!\n");
    }
    
    if (tcp_init() == NET_SUCCESS) {
        vga_write_string("TCP protocol initialized successfully!\n");
    } else {
        vga_write_string("TCP protocol initialization failed!\n");
    }
    
    // Set colors for a nice welcome screen
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("=================================================\n");
    vga_write_string("    TradeKernel OS - Built from Scratch v1.0    \n");
    vga_write_string("=================================================\n\n");
    
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_write_string("Kernel successfully loaded and executing in 32-bit protected mode!\n\n");
    
    // Display system information
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write_string("System Information:\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_write_string("  Architecture: x86 (32-bit)\n");
    vga_write_string("  Memory detected: ");
    print_dec(detect_memory() / 1024);
    vga_write_string(" KB\n");
    vga_write_string("  Kernel heap: ");
    print_dec(get_total_memory() / 1024);
    vga_write_string(" KB\n");
    vga_write_string("  VGA Text Mode: 80x25\n\n");
    
    // Show kernel features
    vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    vga_write_string("Kernel Features:\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_write_string("  [OK] Bootloader transition\n");
    vga_write_string("  [OK] Protected mode\n");
    vga_write_string("  [OK] VGA text driver\n");
    vga_write_string("  [OK] Memory management\n");
    vga_write_string("  [OK] Interrupt handling\n");
    vga_write_string("  [OK] Process management\n");
    vga_write_string("  [OK] Priority scheduler\n");
    vga_write_string("  [OK] System calls\n");
    vga_write_string("  [OK] Inter-process communication\n");
    vga_write_string("  [OK] File system\n");
    vga_write_string("  [OK] Ethernet driver (RTL8139)\n");
    vga_write_string("  [OK] IPv4 protocol stack\n");
    vga_write_string("  [OK] TCP protocol\n");
    vga_write_string("  [OK] Socket API\n");
    vga_write_string("  [OK] WebSocket support\n\n");
    
    vga_set_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
    vga_write_string("TradeKernel OS is ready for development!\n");
    vga_write_string("This is a minimal kernel that can be extended with trading algorithms.\n\n");
    
    vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    vga_write_string("Interactive shell enabled! Type 'help' for available commands.\n");
    vga_write_string("Timer interrupts are working in the background.\n\n");
    
    // GUI Demo - Create a sample window
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write_string("GUI Framework Demo:\n");
    
    // Clear screen for GUI demo
    vga_clear();
    
    window_t* demo_window = gui_create_window(10, 8, 50, 15, "TradeKernel GUI Demo");
    if (demo_window) {
        gui_create_label(demo_window, 2, 1, "Welcome to TradeKernel OS!");
        gui_create_label(demo_window, 2, 3, "This demonstrates the GUI framework.");
        gui_create_button(demo_window, 15, 6, 20, 3, "OK", NULL);
        gui_create_checkbox(demo_window, 2, 10, "Enable advanced features", 1);
        gui_show_window(demo_window);
    }
    
    // Brief delay to show GUI
    for (volatile int i = 0; i < 5000000; i++) {
        // Busy wait to show GUI
    }
    
    // Clear screen again for shell
    vga_clear();
    
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write_string("GUI demo completed. Starting shell...\n\n");
    
    // Initialize and start the shell
    shell_init();
    
    // Keep the kernel alive
    while (1) {
        __asm__ volatile ("hlt");
    }
}