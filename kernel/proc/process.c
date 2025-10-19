#include "process.h"
#include "../mm/memory.h"
#include "../mm/paging.h"
#include "../drivers/vga.h"

// Global variables
process_t process_table[MAX_PROCESSES];
static bool process_table_used[MAX_PROCESSES];
static uint32_t next_pid = 1;
static uint32_t system_time_ms = 0;
process_t* current_process = NULL;
process_t* idle_process = NULL;

// Idle task function - runs when no other processes are ready
void idle_task(void) {
    while (1) {
        __asm__ volatile ("hlt"); // Halt CPU until next interrupt
    }
}

// Process statistics
process_stats_t proc_stats = {0};

// Scheduler queues for each priority level
scheduler_queue_t ready_queues[5];  // One for each priority level
scheduler_queue_t blocked_queue;
scheduler_queue_t terminated_queue;

// Scheduler state
bool scheduler_enabled = false;
uint32_t scheduler_ticks = 0;
static uint32_t load_calculation_timer = 0;

// Initialize process management system
void process_init(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("Initializing process management...\n");
    
    // Clear process table
    memset(process_table, 0, sizeof(process_table));
    memset(process_table_used, 0, sizeof(process_table_used));
    
    // Initialize scheduler queues
    for (int i = 0; i < 5; i++) {
        queue_init(&ready_queues[i]);
    }
    queue_init(&blocked_queue);
    queue_init(&terminated_queue);
    
    // Initialize statistics
    memset(&proc_stats, 0, sizeof(proc_stats));
    
    // Create idle process
    idle_process = process_create("idle", idle_task, PRIORITY_IDLE);
    if (idle_process) {
        idle_process->pid = IDLE_PROCESS_PID;
        idle_process->state = PROCESS_READY;
    }
    
    // Set initial current process to idle
    current_process = idle_process;
    
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write_string("Process management initialized\n");
}

// Create a new process
process_t* process_create(const char* name, void* entry_point, process_priority_t priority) {
    // Find free slot in process table
    int slot = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (!process_table_used[i]) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        return NULL; // No free slots
    }
    
    process_t* process = &process_table[slot];
    process_table_used[slot] = true;
    
    // Initialize process structure
    memset(process, 0, sizeof(process_t));
    
    // Basic process information
    process->pid = process_get_next_pid();
    process->ppid = current_process ? current_process->pid : 0;
    strncpy(process->name, name, sizeof(process->name) - 1);
    process->name[sizeof(process->name) - 1] = '\0';
    
    // Process state
    process->state = PROCESS_NEW;
    process->priority = priority;
    process->policy = SCHED_RR; // Default to round-robin
    
    // Timing information
    process->creation_time = get_current_time_ms();
    process->cpu_time = 0;
    process->last_run_time = 0;
    process->time_slice = DEFAULT_TIME_SLICE;
    process->remaining_slice = DEFAULT_TIME_SLICE;
    
    // Memory setup
    process->stack_size = DEFAULT_STACK_SIZE;
    process->stack_base = (uint32_t)kmalloc(process->stack_size);
    if (!process->stack_base) {
        process_table_used[slot] = false;
        return NULL;
    }
    
    // Initialize CPU context
    memset(&process->context, 0, sizeof(cpu_context_t));
    process->context.eip = (uint32_t)entry_point;
    process->context.esp = process->stack_base + process->stack_size - 4;
    process->context.ebp = process->context.esp;
    process->context.eflags = 0x202; // Enable interrupts
    
    // Set up segments (assuming flat memory model)
    process->context.cs = 0x08; // Kernel code segment
    process->context.ds = 0x10; // Kernel data segment
    process->context.es = 0x10;
    process->context.fs = 0x10;
    process->context.gs = 0x10;
    process->context.ss = 0x10;
    
    // Initialize file descriptor table
    for (int i = 0; i < 32; i++) {
        process->fd_table[i] = -1; // Closed
    }
    
    // Set up process relationships
    if (current_process) {
        process->parent = current_process;
        // Add to parent's children list
        if (current_process->children) {
            process->sibling_next = current_process->children;
            current_process->children->sibling_prev = process;
        }
        current_process->children = process;
    }
    
    // Update statistics
    proc_stats.total_processes++;
    proc_stats.active_processes++;
    
    return process;
}

// Destroy a process and free its resources
int process_destroy(process_t* process) {
    if (!process) return -1;
    
    // Remove from parent's children list
    if (process->parent) {
        if (process->parent->children == process) {
            process->parent->children = process->sibling_next;
        }
        if (process->sibling_prev) {
            process->sibling_prev->sibling_next = process->sibling_next;
        }
        if (process->sibling_next) {
            process->sibling_next->sibling_prev = process->sibling_prev;
        }
    }
    
    // Terminate all child processes
    process_t* child = process->children;
    while (child) {
        process_t* next_child = child->sibling_next;
        process_kill(child->pid, 9); // SIGKILL
        child = next_child;
    }
    
    // Free memory resources
    if (process->stack_base) {
        kfree((void*)process->stack_base);
    }
    
    // Remove from scheduler queues
    scheduler_remove_process(process);
    
    // Mark process table slot as free
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (&process_table[i] == process) {
            process_table_used[i] = false;
            break;
        }
    }
    
    // Update statistics
    proc_stats.active_processes--;
    
    return 0;
}

// Find process by PID
process_t* process_find_by_pid(uint32_t pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table_used[i] && process_table[i].pid == pid) {
            return &process_table[i];
        }
    }
    return NULL;
}

// Get next available PID
uint32_t process_get_next_pid(void) {
    uint32_t pid = next_pid;
    next_pid++;
    if (next_pid >= MAX_PROCESSES) {
        next_pid = 1; // Don't reuse PID 0 (idle)
    }
    return pid;
}

// Process exit
void process_exit(process_t* process, int32_t exit_code) {
    if (!process) return;
    
    process->exit_code = exit_code;
    process_set_state(process, PROCESS_TERMINATED);
    
    // If this is the current process, schedule next one
    if (process == current_process) {
        scheduler_yield();
    }
}

// Kill a process by PID
int process_kill(uint32_t pid, int32_t signal) {
    process_t* process = process_find_by_pid(pid);
    if (!process) return -1;
    
    // For now, just terminate the process
    // TODO: Handle different signals properly
    process_exit(process, -signal);
    return 0;
}

// Set process state
void process_set_state(process_t* process, process_state_t new_state) {
    if (!process || process->state == new_state) return;
    
    process_state_t old_state = process->state;
    process->state = new_state;
    
    // Update statistics
    switch (old_state) {
        case PROCESS_RUNNING:
            proc_stats.running_processes--;
            break;
        case PROCESS_BLOCKED:
            proc_stats.blocked_processes--;
            break;
        default:
            break;
    }
    
    switch (new_state) {
        case PROCESS_RUNNING:
            proc_stats.running_processes++;
            break;
        case PROCESS_BLOCKED:
            proc_stats.blocked_processes++;
            break;
        case PROCESS_READY:
            scheduler_add_process(process);
            break;
        case PROCESS_TERMINATED:
            queue_add_tail(&terminated_queue, process);
            break;
        default:
            break;
    }
}

// Block a process
void process_block(process_t* process) {
    if (!process) return;
    
    scheduler_remove_process(process);
    process_set_state(process, PROCESS_BLOCKED);
    queue_add_tail(&blocked_queue, process);
    
    if (process == current_process) {
        scheduler_yield();
    }
}

// Unblock a process
void process_unblock(process_t* process) {
    if (!process || process->state != PROCESS_BLOCKED) return;
    
    queue_remove(&blocked_queue, process);
    process_set_state(process, PROCESS_READY);
}

// Sleep process for specified milliseconds
void process_sleep(process_t* process, uint32_t ms) {
    (void)ms;  // Suppress unused parameter warning - TODO: implement timer-based sleep
    if (!process) return;
    
    // For now, just block the process
    // In a full implementation, we'd set a timer
    process_block(process);
}

// Set process priority
void process_set_priority(process_t* process, process_priority_t priority) {
    if (!process) return;
    
    // Remove from current queue
    if (process->state == PROCESS_READY) {
        scheduler_remove_process(process);
    }
    
    process->priority = priority;
    
    // Add back to appropriate queue
    if (process->state == PROCESS_READY) {
        scheduler_add_process(process);
    }
}

void process_show_all_processes(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state != PROCESS_NEW) {
            // Format: PID  PPID PRIO STATE    CPU%  MEMORY  NAME
            print_number(process_table[i].pid);
            vga_write_string("   ");
            print_number(process_table[i].ppid);
            vga_write_string("  ");
            print_number(process_table[i].priority);
            vga_write_string("   ");
            
            switch (process_table[i].state) {
                case PROCESS_RUNNING: vga_write_string("RUNNING "); break;
                case PROCESS_READY: vga_write_string("READY   "); break;
                case PROCESS_BLOCKED: vga_write_string("BLOCKED "); break;
                case PROCESS_SLEEPING: vga_write_string("SLEEPING"); break;
                case PROCESS_ZOMBIE: vga_write_string("ZOMBIE  "); break;
                default: vga_write_string("UNKNOWN "); break;
            }
            
            vga_write_string("  ");
            print_number(process_table[i].cpu_time);
            vga_write_string("   ");
            print_number(process_table[i].memory_used);
            vga_write_string("  ");
            vga_write_string(process_table[i].name);
            vga_write_string("\n");
        }
    }
}

// Get current system time in milliseconds
uint32_t get_current_time_ms(void) {
    return system_time_ms;
}

// Update system time (called from timer interrupt)
void system_tick(void) {
    system_time_ms += (1000 / SCHEDULER_FREQUENCY); // Assuming 100Hz timer
    scheduler_ticks++;
    
    // Update load average calculation
    load_calculation_timer++;
    if (load_calculation_timer >= SCHEDULER_FREQUENCY) { // Every second
        load_calculation_timer = 0;
        // Simple load average calculation
        proc_stats.load_average = (proc_stats.running_processes + ready_queues[PRIORITY_REALTIME].count + 
                                   ready_queues[PRIORITY_HIGH].count) * 100;
    }
}

// (idle_task is defined earlier in file)

// Get process statistics
void get_process_stats(process_stats_t* stats) {
    if (stats) {
        *stats = proc_stats;
        stats->scheduler_ticks = scheduler_ticks;
    }
}

// Print process list
void print_process_list(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("=== Process List ===\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_write_string("PID  PPID NAME         STATE    PRIORITY CPU_TIME\n");
    
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table_used[i]) {
            process_t* p = &process_table[i];
            
            // Print PID
            print_number(p->pid);
            vga_write_string("  ");
            
            // Print PPID
            print_number(p->ppid);
            vga_write_string("  ");
            
            // Print name (truncated to 12 chars)
            char name_buf[13];
            strncpy(name_buf, p->name, 12);
            name_buf[12] = '\0';
            vga_write_string(name_buf);
            
            // Pad name field
            int name_len = strlen(name_buf);
            for (int j = name_len; j < 12; j++) {
                vga_putchar(' ');
            }
            vga_putchar(' ');
            
            // Print state
            switch (p->state) {
                case PROCESS_NEW: vga_write_string("NEW    "); break;
                case PROCESS_READY: vga_write_string("READY  "); break;
                case PROCESS_RUNNING: vga_write_string("RUN    "); break;
                case PROCESS_BLOCKED: vga_write_string("BLOCK  "); break;
                case PROCESS_SLEEPING: vga_write_string("SLEEP  "); break;
                case PROCESS_ZOMBIE: vga_write_string("ZOMBIE "); break;
                case PROCESS_TERMINATED: vga_write_string("TERM   "); break;
            }
            
            // Print priority
            print_number(p->priority);
            vga_write_string("        ");
            
            // Print CPU time
            print_number(p->cpu_time);
            vga_write_string("ms\n");
        }
    }
    vga_write_string("\n");
}