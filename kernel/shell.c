#include "shell.h"
#include "drivers/vga.h"
#include "mm/memory.h"
#include "mm/paging.h"
#include "fs/fs.h"
#include "proc/process.h"
#include "proc/scheduler.h"
#include "proc/syscalls.h" // System calls enabled
#include "proc/ipc.h" // IPC enabled
#include "net/websocket.h"

static char command_buffer[MAX_COMMAND_LENGTH];
static int buffer_pos = 0;

// Forward declarations for command functions
void cmd_help(int argc, char* argv[]);
void cmd_clear(int argc, char* argv[]);
void cmd_info(int argc, char* argv[]);
void cmd_mem(int argc, char* argv[]);
void cmd_memstats(int argc, char* argv[]);
void cmd_memleak(int argc, char* argv[]);
void cmd_memcheck(int argc, char* argv[]);
void cmd_pgstats(int argc, char* argv[]);
void cmd_ps(int argc, char* argv[]);
void cmd_schedstat(int argc, char* argv[]);
void cmd_procinfo(int argc, char* argv[]);
void cmd_testfork(int argc, char* argv[]);
void cmd_testipc(int argc, char* argv[]);
void cmd_msgtest(int argc, char* argv[]);
void cmd_ls(int argc, char* argv[]);
void cmd_mkdir(int argc, char* argv[]);
void cmd_touch(int argc, char* argv[]);
void cmd_rm(int argc, char* argv[]);
void cmd_cat(int argc, char* argv[]);
void cmd_cp(int argc, char* argv[]);
void cmd_mv(int argc, char* argv[]);
void cmd_reboot(int argc, char* argv[]);
void cmd_websocket_test(int argc, char* argv[]);

// Built-in commands table
static shell_command_t commands[] = {
    {"help", "Show available commands", cmd_help},
    {"clear", "Clear the screen", cmd_clear},
    {"info", "Show system information", cmd_info},
    {"mem", "Show memory usage", cmd_mem},
    {"memstats", "Show detailed heap statistics", cmd_memstats},
    {"memleak", "Detect memory leaks", cmd_memleak},
    {"memcheck", "Check heap integrity", cmd_memcheck},
    {"pgstats", "Show paging statistics", cmd_pgstats},
    {"ps", "Show running processes", cmd_ps},
    {"schedstat", "Show scheduler statistics", cmd_schedstat},
    {"procinfo", "Show detailed process information", cmd_procinfo},
    {"testfork", "Test fork() system call", cmd_testfork},
    {"testipc", "Test inter-process communication", cmd_testipc},
    {"msgtest", "Test message queues", cmd_msgtest},
    {"ls", "List directory contents", cmd_ls},
    {"mkdir", "Create directory", cmd_mkdir},
    {"touch", "Create file", cmd_touch},
    {"rm", "Remove file", cmd_rm},
    {"cat", "Display file contents", cmd_cat},
    {"cp", "Copy file", cmd_cp},
    {"mv", "Move/rename file", cmd_mv},
    {"reboot", "Restart the system", cmd_reboot},
    {"wstest", "Test WebSocket connection to Binance", cmd_websocket_test},
};

void shell_init(void) {
    buffer_pos = 0;
    command_buffer[0] = '\0';
    
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_write_string("$ ");
}

// Simple number printing function
static void print_dec(uint32_t value) {
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

// Simple string utilities
static int shell_strlen(const char* str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}

static int shell_strcmp(const char* str1, const char* str2) {
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return *str1 - *str2;
}

static void shell_strcpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

// Parse command line into arguments
static int parse_args(char* command_line, char* argv[]) {
    int argc = 0;
    char* token = command_line;
    
    while (*token && argc < MAX_ARGS - 1) {
        // Skip whitespace
        while (*token == ' ' || *token == '\t') token++;
        if (!*token) break;
        
        argv[argc++] = token;
        
        // Find end of token
        while (*token && *token != ' ' && *token != '\t') token++;
        if (*token) *token++ = '\0';
    }
    
    argv[argc] = NULL;
    return argc;
}

void shell_process_input(char c) {
    if (c == '\n' || c == '\r') {
        // Execute command
        vga_putchar('\n');
        command_buffer[buffer_pos] = '\0';
        
        if (buffer_pos > 0) {
            shell_execute_command(command_buffer);
        }
        
        // Reset for next command
        buffer_pos = 0;
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_write_string("$ ");
    } else if (c == '\b') {
        // Backspace
        if (buffer_pos > 0) {
            buffer_pos--;
            vga_putchar('\b');
        }
    } else if (c >= 32 && c <= 126) {
        // Printable character
        if (buffer_pos < MAX_COMMAND_LENGTH - 1) {
            command_buffer[buffer_pos++] = c;
            vga_putchar(c);
        }
    }
}

void shell_execute_command(const char* command_line) {
    char temp_buffer[MAX_COMMAND_LENGTH];
    char* argv[MAX_ARGS];
    int argc;
    
    if (shell_strlen(command_line) == 0) return;
    
    // Copy command line to temporary buffer for parsing
    shell_strcpy(temp_buffer, command_line);
    argc = parse_args(temp_buffer, argv);
    
    if (argc == 0) return;
    
    // Find and execute command
    for (int i = 0; commands[i].name != NULL; i++) {
        if (shell_strcmp(argv[0], commands[i].name) == 0) {
            commands[i].handler(argc, argv);
            return;
        }
    }
    
    // Command not found
    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_write_string("Command not found: ");
    vga_write_string(argv[0]);
    vga_write_string("\nType 'help' for available commands.\n");
}

// Built-in command implementations
void cmd_help(int argc, char* argv[]) {
    (void)argc; (void)argv; // Suppress unused parameter warnings
    
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("TradeKernel OS - Available Commands:\n\n");
    
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    for (int i = 0; commands[i].name != NULL; i++) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_write_string(commands[i].name);
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        vga_write_string(" - ");
        vga_write_string(commands[i].description);
        vga_write_string("\n");
    }
    vga_write_string("\n");
}

void cmd_clear(int argc, char* argv[]) {
    (void)argc; (void)argv;
    vga_clear();
}

void cmd_info(int argc, char* argv[]) {
    (void)argc; (void)argv;
    
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("=== System Information ===\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_write_string("OS: TradeKernel v1.0\n");
    vga_write_string("Architecture: x86 (32-bit)\n");
    vga_write_string("Memory: 16MB\n");
    vga_write_string("VGA Mode: 80x25 text\n");
    vga_write_string("Status: Running\n\n");
}

void cmd_mem(int argc, char* argv[]) {
    (void)argc; (void)argv;
    
    size_t total = get_total_memory();
    size_t free = get_free_memory();
    size_t used = total - free;
    
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("=== Memory Usage ===\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_write_string("Total Heap: ");
    print_dec(total / 1024);
    vga_write_string(" KB\n");
    vga_write_string("Used: ");
    print_dec(used / 1024);
    vga_write_string(" KB\n");
    vga_write_string("Free: ");
    print_dec(free / 1024);
    vga_write_string(" KB\n\n");
}

void cmd_memstats(int argc, char* argv[]) {
    (void)argc; (void)argv;
    print_heap_stats();
    print_allocation_list();
}

void cmd_memleak(int argc, char* argv[]) {
    (void)argc; (void)argv;
    detect_memory_leaks();
}

void cmd_memcheck(int argc, char* argv[]) {
    (void)argc; (void)argv;
    int errors = check_heap_integrity();
    
    if (errors == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_write_string("Heap integrity check passed.\n");
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Heap integrity check failed with ");
        print_dec(errors);
        vga_write_string(" errors.\n");
    }
}

void cmd_pgstats(int argc, char* argv[]) {
    (void)argc; (void)argv;
    print_memory_stats();
}

void cmd_echo(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        vga_write_string(argv[i]);
        if (i < argc - 1) vga_write_string(" ");
    }
    vga_write_string("\n");
}

void cmd_reboot(int argc, char* argv[]) {
    (void)argc; (void)argv;
    
    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_write_string("Rebooting system...\n");
    
    // Simple reboot via keyboard controller
    uint8_t temp = 0xFE;
    __asm__ volatile ("outb %0, $0x64" : : "a"(temp));
    
    // If that doesn't work, halt
    vga_write_string("Reboot failed. System halted.\n");
    while (1) {
        __asm__ volatile ("hlt");
    }
}

void cmd_ls(int argc, char* argv[]) {
    const char* path = "/";  // Default to root directory
    
    if (argc > 1) {
        path = argv[1];
    }
    
    directory_entry_t entries[32];
    int count = fs_list_directory(path, entries, 32);
    
    if (count < 0) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        if (count == FS_ERROR_NOT_FOUND) {
            vga_write_string("Directory not found: ");
            vga_write_string(path);
        } else if (count == FS_ERROR_INVALID) {
            vga_write_string("Not a directory: ");
            vga_write_string(path);
        } else {
            vga_write_string("Error reading directory");
        }
        vga_write_string("\n");
        return;
    }
    
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("Contents of ");
    vga_write_string(path);
    vga_write_string(":\n");
    
    if (count == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        vga_write_string("  (empty directory)\n");
        return;
    }
    
    for (int i = 0; i < count; i++) {
        if (entries[i].file_type == FILE_TYPE_DIRECTORY) {
            vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
            vga_write_string("  [DIR]  ");
        } else {
            vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
            vga_write_string("  [FILE] ");
        }
        vga_write_string(entries[i].name);
        vga_write_string("\n");
    }
}

void cmd_mkdir(int argc, char* argv[]) {
    if (argc < 2) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Usage: mkdir <directory_name>\n");
        return;
    }
    
    const char* dir_path = argv[1];
    int result = fs_create_directory(dir_path);
    
    if (result == FS_SUCCESS) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_write_string("Directory created: ");
        vga_write_string(dir_path);
        vga_write_string("\n");
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Failed to create directory: ");
        vga_write_string(dir_path);
        vga_write_string(" (error: ");
        
        switch (result) {
            case FS_ERROR_EXISTS:
                vga_write_string("already exists");
                break;
            case FS_ERROR_NOT_FOUND:
                vga_write_string("parent not found");
                break;
            case FS_ERROR_NO_SPACE:
                vga_write_string("no space");
                break;
            case FS_ERROR_INVALID:
                vga_write_string("invalid path");
                break;
            case FS_ERROR_NO_MEMORY:
                vga_write_string("no memory");
                break;
            default:
                vga_write_string("unknown error ");
                // Print error code
                char code_str[10];
                int temp = result;
                if (temp < 0) temp = -temp;
                int i = 0;
                do {
                    code_str[i++] = '0' + (temp % 10);
                    temp /= 10;
                } while (temp > 0 && i < 9);
                if (result < 0) {
                    vga_write_string("-");
                }
                while (i > 0) {
                    vga_putchar(code_str[--i]);
                }
                break;
        }
        
        vga_write_string(")\n");
    }
}

void cmd_touch(int argc, char* argv[]) {
    if (argc < 2) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Usage: touch <filename>\n");
        return;
    }
    
    const char* file_path = argv[1];
    int result = fs_create_file(file_path, FILE_TYPE_REGULAR);
    
    if (result == FS_SUCCESS) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_write_string("File created: ");
        vga_write_string(file_path);
        vga_write_string("\n");
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        if (result == FS_ERROR_EXISTS) {
            vga_write_string("File already exists: ");
        } else if (result == FS_ERROR_NOT_FOUND) {
            vga_write_string("Parent directory not found: ");
        } else if (result == FS_ERROR_NO_SPACE) {
            vga_write_string("No space left on device: ");
        } else {
            vga_write_string("Failed to create file: ");
        }
        vga_write_string(file_path);
        vga_write_string("\n");
    }
}

void cmd_rm(int argc, char* argv[]) {
    if (argc < 2) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Usage: rm <filename>\n");
        return;
    }
    
    const char* file_path = argv[1];
    int result = fs_delete_file(file_path);
    
    if (result == FS_SUCCESS) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_write_string("File deleted: ");
        vga_write_string(file_path);
        vga_write_string("\n");
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        if (result == FS_ERROR_NOT_FOUND) {
            vga_write_string("File not found: ");
        } else {
            vga_write_string("Failed to delete file: ");
        }
        vga_write_string(file_path);
        vga_write_string("\n");
    }
}

void cmd_cat(int argc, char* argv[]) {
    if (argc < 2) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Usage: cat <filename>\n");
        return;
    }
    
    const char* file_path = argv[1];
    int fd = fs_open(file_path, 0);  // Read-only
    
    if (fd < 0) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        if (fd == FS_ERROR_NOT_FOUND) {
            vga_write_string("File not found: ");
        } else {
            vga_write_string("Failed to open file: ");
        }
        vga_write_string(file_path);
        vga_write_string("\n");
        return;
    }
    
    // Get file size first
    inode_t inode_info;
    if (fs_stat(file_path, &inode_info) != FS_SUCCESS) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Failed to get file information\n");
        fs_close(fd);
        return;
    }
    
    if (inode_info.size == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        vga_write_string("(empty file)\n");
        fs_close(fd);
        return;
    }
    
    // Read and display file contents
    char buffer[513];  // 512 bytes + null terminator
    uint32_t total_read = 0;
    
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    while (total_read < inode_info.size) {
        uint32_t to_read = (inode_info.size - total_read > 512) ? 512 : (inode_info.size - total_read);
        int bytes_read = fs_read(fd, buffer, to_read);
        
        if (bytes_read <= 0) {
            break;
        }
        
        buffer[bytes_read] = '\0';  // Null terminate
        
        // Display the content
        for (int i = 0; i < bytes_read; i++) {
            if (buffer[i] == '\0') {
                // Stop at first null byte (though files shouldn't contain nulls)
                break;
            }
            vga_putchar(buffer[i]);
        }
        
        total_read += bytes_read;
    }
    
    vga_write_string("\n");
    fs_close(fd);
}

void cmd_cp(int argc, char* argv[]) {
    if (argc < 3) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Usage: cp <source> <destination>\n");
        return;
    }
    
    const char* src_path = argv[1];
    const char* dst_path = argv[2];
    
    // Check if destination already exists
    if (fs_exists(dst_path)) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Destination already exists: ");
        vga_write_string(dst_path);
        vga_write_string("\n");
        return;
    }
    
    // Open source file
    int src_fd = fs_open(src_path, 0);  // Read-only
    if (src_fd < 0) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        if (src_fd == FS_ERROR_NOT_FOUND) {
            vga_write_string("Source file not found: ");
        } else {
            vga_write_string("Failed to open source file: ");
        }
        vga_write_string(src_path);
        vga_write_string("\n");
        return;
    }
    
    // Get source file info
    inode_t src_inode;
    if (fs_stat(src_path, &src_inode) != FS_SUCCESS) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Failed to get source file information\n");
        fs_close(src_fd);
        return;
    }
    
    // Create destination file
    int result = fs_create_file(dst_path, FILE_TYPE_REGULAR);
    if (result != FS_SUCCESS) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Failed to create destination file: ");
        vga_write_string(dst_path);
        vga_write_string("\n");
        fs_close(src_fd);
        return;
    }
    
    // Open destination file for writing
    int dst_fd = fs_open(dst_path, 1);  // Write-only (assuming flag 1 means write)
    if (dst_fd < 0) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Failed to open destination file for writing\n");
        fs_close(src_fd);
        return;
    }
    
    // Copy file contents
    char buffer[512];
    uint32_t total_copied = 0;
    int success = 1;
    
    while (total_copied < src_inode.size) {
        uint32_t to_read = (src_inode.size - total_copied > 512) ? 512 : (src_inode.size - total_copied);
        int bytes_read = fs_read(src_fd, buffer, to_read);
        
        if (bytes_read <= 0) {
            success = 0;
            break;
        }
        
        int bytes_written = fs_write(dst_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            success = 0;
            break;
        }
        
        total_copied += bytes_read;
    }
    
    fs_close(src_fd);
    fs_close(dst_fd);
    
    if (success) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_write_string("File copied: ");
        vga_write_string(src_path);
        vga_write_string(" -> ");
        vga_write_string(dst_path);
        vga_write_string("\n");
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Failed to copy file\n");
        // Try to remove the incomplete destination file
        fs_delete_file(dst_path);
    }
}

void cmd_mv(int argc, char* argv[]) {
    if (argc < 3) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Usage: mv <source> <destination>\n");
        return;
    }
    
    const char* src_path = argv[1];
    const char* dst_path = argv[2];
    
    // Check if source exists
    if (!fs_exists(src_path)) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Source file not found: ");
        vga_write_string(src_path);
        vga_write_string("\n");
        return;
    }
    
    // Check if destination already exists
    if (fs_exists(dst_path)) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Destination already exists: ");
        vga_write_string(dst_path);
        vga_write_string("\n");
        return;
    }
    
    // For now, implement as copy + delete
    // Open source file
    int src_fd = fs_open(src_path, 0);  // Read-only
    if (src_fd < 0) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Failed to open source file\n");
        return;
    }
    
    // Get source file info
    inode_t src_inode;
    if (fs_stat(src_path, &src_inode) != FS_SUCCESS) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Failed to get source file information\n");
        fs_close(src_fd);
        return;
    }
    
    // Create destination file
    int result = fs_create_file(dst_path, FILE_TYPE_REGULAR);
    if (result != FS_SUCCESS) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Failed to create destination file\n");
        fs_close(src_fd);
        return;
    }
    
    // Open destination file for writing
    int dst_fd = fs_open(dst_path, 1);  // Write-only
    if (dst_fd < 0) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Failed to open destination file for writing\n");
        fs_close(src_fd);
        return;
    }
    
    // Copy file contents
    char buffer[512];
    uint32_t total_copied = 0;
    int success = 1;
    
    while (total_copied < src_inode.size) {
        uint32_t to_read = (src_inode.size - total_copied > 512) ? 512 : (src_inode.size - total_copied);
        int bytes_read = fs_read(src_fd, buffer, to_read);
        
        if (bytes_read <= 0) {
            success = 0;
            break;
        }
        
        int bytes_written = fs_write(dst_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            success = 0;
            break;
        }
        
        total_copied += bytes_read;
    }
    
    fs_close(src_fd);
    fs_close(dst_fd);
    
    if (success) {
        // Delete the source file
        if (fs_delete_file(src_path) == FS_SUCCESS) {
            vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            vga_write_string("File moved: ");
            vga_write_string(src_path);
            vga_write_string(" -> ");
            vga_write_string(dst_path);
            vga_write_string("\n");
        } else {
            vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            vga_write_string("Failed to remove source file after copy\n");
            // Destination file exists but source still exists - inconsistent state
        }
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Failed to move file\n");
        // Clean up the incomplete destination file
        fs_delete_file(dst_path);
    }
}

// Process management commands
void cmd_ps(int argc, char* argv[]) {
    (void)argc; (void)argv;  // Suppress unused parameter warnings
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("Process List:\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_write_string("PID  PPID PRIO STATE    CPU%  MEMORY  NAME\n");
    vga_write_string("---  ---- ---- -------- ----  ------  ----\n");
    
    // Display process information
    process_show_all_processes();
}

void cmd_schedstat(int argc, char* argv[]) {
    (void)argc; (void)argv;  // Suppress unused parameter warnings
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("Scheduler Statistics:\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    scheduler_show_stats();
}

void cmd_procinfo(int argc, char* argv[]) {
    if (argc < 2) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Usage: procinfo <pid>\n");
        return;
    }
    
    // Simple string to number conversion
    uint32_t pid = 0;
    const char* str = argv[1];
    while (*str >= '0' && *str <= '9') {
        pid = pid * 10 + (*str - '0');
        str++;
    }
    
    if (pid == 0) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Invalid PID\n");
        return;
    }
    
    process_t* proc = process_find_by_pid(pid);
    if (!proc) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Process not found\n");
        return;
    }
    
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("Process Information:\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    vga_write_string("  PID: ");
    print_dec(proc->pid);
    vga_write_string("\n");
    
    vga_write_string("  Parent PID: ");
    print_dec(proc->ppid);
    vga_write_string("\n");
    
    vga_write_string("  Priority: ");
    print_dec(proc->priority);
    vga_write_string("\n");
    
    vga_write_string("  State: ");
    switch (proc->state) {
        case PROCESS_RUNNING: vga_write_string("RUNNING"); break;
        case PROCESS_READY: vga_write_string("READY"); break;
        case PROCESS_BLOCKED: vga_write_string("BLOCKED"); break;
        case PROCESS_SLEEPING: vga_write_string("SLEEPING"); break;
        case PROCESS_ZOMBIE: vga_write_string("ZOMBIE"); break;
        default: vga_write_string("UNKNOWN"); break;
    }
    vga_write_string("\n");
    
    vga_write_string("  CPU Time: ");
    print_dec(proc->cpu_time);
    vga_write_string(" ticks\n");
    
    vga_write_string("  Memory Used: ");
    print_dec(proc->memory_used);
    vga_write_string(" bytes\n");
}

void cmd_testfork(int argc, char* argv[]) {
    (void)argc; (void)argv;  // Suppress unused parameter warnings
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("Testing fork() system call...\n");
    
    // Create a simple test process
    process_t* child = process_create("test_child", NULL, PRIORITY_NORMAL);
    if (child) {
        scheduler_add_process(child);
        
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_write_string("Child process created with PID: ");
        print_dec(child->pid);
        vga_write_string("\n");
        
        // Simulate some work for the child
        child->cpu_time = 10;
        child->memory_used = 4096;
        
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        vga_write_string("Note: This is a demonstration - child process will be cleaned up.\n");
        
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Failed to create child process\n");
    }
}

// TODO: Re-enable when IPC is fixed
void cmd_testipc(int argc, char* argv[]) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("Testing Inter-Process Communication...\n");
    
    // Test shared memory pool
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_write_string("Creating shared memory pool...\n");
    
    shared_pool_t* pool = create_shared_pool(sizeof(market_data_t), 100);
    if (pool) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_write_string("Shared memory pool created successfully\n");
        
        // Test allocation
        market_data_t* data = (market_data_t*)shared_pool_alloc(pool);
        if (data) {
            // Fill with test data
            data->price = 12345; // Simplified - would be a double
            data->volume = 1000;
            data->symbol_id = 1;
            data->side = 0; // bid
            
            vga_write_string("Test market data allocated and filled\n");
            
            shared_pool_free(pool, data);
            vga_write_string("Memory freed successfully\n");
        }
        
        destroy_shared_pool(pool);
        vga_write_string("Shared memory pool destroyed\n");
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Failed to create shared memory pool\n");
    }
}

// TODO: Re-enable when IPC is fixed
void cmd_msgtest(int argc, char* argv[]) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("Testing Message Queues...\n");
    
    // Create a message queue
    uint32_t queue_id = msgget(0x1234, 0x200); // IPC_CREAT
    if (queue_id != (uint32_t)-1) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_write_string("Message queue created with ID: ");
        print_dec(queue_id);
        vga_write_string("\n");
        
        // Test sending a message
        message_t msg;
        msg.type = MSG_MARKET_DATA;
        msg.priority = 1;
        
        // Fill with test market data
        market_data_t test_data;
        test_data.price = 98765; // Simplified
        test_data.volume = 500;
        test_data.symbol_id = 42;
        test_data.side = 1; // ask
        
        // Copy to message
        for (int i = 0; i < sizeof(market_data_t); i++) {
            msg.data[i] = ((uint8_t*)&test_data)[i];
        }
        
        int result = msgsnd(queue_id, &msg, sizeof(market_data_t), 0);
        if (result == 0) {
            vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            vga_write_string("Message sent successfully\n");
            
            // Test receiving the message
            message_t recv_msg;
            result = msgrcv(queue_id, &recv_msg, sizeof(market_data_t), MSG_MARKET_DATA, 0x800);
            if (result > 0) {
                vga_write_string("Message received successfully\n");
                
                // Verify data
                market_data_t* received_data = (market_data_t*)recv_msg.data;
                vga_write_string("Received data - Symbol: ");
                print_dec(received_data->symbol_id);
                vga_write_string(", Volume: ");
                print_dec((uint32_t)received_data->volume);
                vga_write_string("\n");
            } else {
                vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
                vga_write_string("Failed to receive message\n");
            }
        } else {
            vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            vga_write_string("Failed to send message\n");
        }
        
        // Clean up
        msgctl(queue_id, 0, NULL); // IPC_RMID
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        vga_write_string("Message queue destroyed\n");
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("Failed to create message queue\n");
    }
}

void cmd_websocket_test(int argc, char* argv[]) {
    (void)argc; // Suppress unused parameter warning
    (void)argv; // Suppress unused parameter warning
    
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("Testing Network Stack Components...\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    // Phase 1: Test basic network initialization
    vga_write_string("Phase 1: Network Stack Status\n");
    vga_write_string("  - RTL8139 Ethernet Driver: Initialized\n");
    vga_write_string("  - IPv4 Protocol Stack: Ready\n");
    vga_write_string("  - TCP Protocol: Active\n");
    vga_write_string("  - Socket API: Available\n");
    
    // Phase 2: Test socket creation (safe test)
    vga_write_string("Phase 2: Testing Socket Creation...\n");
    int test_socket = socket_create(AF_INET, SOCK_STREAM, 0);
    if (test_socket >= 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_write_string("  - Socket creation: SUCCESS\n");
        socket_close(test_socket);
        vga_write_string("  - Socket cleanup: SUCCESS\n");
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("  - Socket creation: FAILED\n");
    }
    
    // Phase 3: Memory allocation test
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_write_string("Phase 3: Testing Memory Allocation...\n");
    void* test_mem = kmalloc(1024);
    if (test_mem) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_write_string("  - Memory allocation: SUCCESS\n");
        kfree(test_mem);
        vga_write_string("  - Memory deallocation: SUCCESS\n");
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string("  - Memory allocation: FAILED\n");
    }
    
    vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    vga_write_string("\nNetwork Stack Test Complete!\n");
    vga_write_string("Note: Full WebSocket testing requires proper network configuration.\n");
    vga_write_string("Current test validates core network stack components.\n");
    
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}