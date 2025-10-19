#ifndef PROCESS_H
#define PROCESS_H

#include "../types.h"

// Process states
typedef enum {
    PROCESS_NEW = 0,        // Process being created  
    PROCESS_READY,          // Ready to run
    PROCESS_RUNNING,        // Currently executing
    PROCESS_BLOCKED,        // Waiting for I/O or resource
    PROCESS_SLEEPING,       // Sleeping for a specific time
    PROCESS_ZOMBIE,         // Process finished but not cleaned up
    PROCESS_TERMINATED      // Process finished
} process_state_t;

// Process priorities (lower number = higher priority)
typedef enum {
    PRIORITY_REALTIME = 0,  // Critical trading algorithms
    PRIORITY_HIGH = 1,      // Important trading processes
    PRIORITY_NORMAL = 2,    // Standard processes
    PRIORITY_LOW = 3,       // Background tasks
    PRIORITY_IDLE = 4       // Idle processes
} process_priority_t;

// Process scheduling policies
typedef enum {
    SCHED_FIFO = 0,         // First-In-First-Out (real-time)
    SCHED_RR,               // Round-Robin (time-sliced)
    SCHED_NORMAL            // Standard priority-based
} sched_policy_t;

// CPU context structure for context switching
typedef struct cpu_context {
    uint32_t eax, ebx, ecx, edx;    // General purpose registers
    uint32_t esi, edi;              // Index registers
    uint32_t esp, ebp;              // Stack pointers
    uint32_t eip;                   // Instruction pointer
    uint32_t eflags;                // Flags register
    uint16_t cs, ds, es, fs, gs, ss; // Segment registers
    uint32_t cr3;                   // Page directory (if paging enabled)
} __attribute__((packed)) cpu_context_t;

// Process Control Block (PCB)
typedef struct process {
    uint32_t pid;                   // Process ID
    uint32_t ppid;                  // Parent Process ID
    char name[32];                  // Process name
    
    process_state_t state;          // Current state
    process_priority_t priority;    // Process priority
    sched_policy_t policy;          // Scheduling policy
    
    cpu_context_t context;          // Saved CPU context
    
    // Memory management
    uint32_t* page_directory;       // Virtual memory space
    uint32_t stack_base;            // Stack base address
    uint32_t stack_size;            // Stack size
    uint32_t heap_base;             // Heap base address
    uint32_t heap_size;             // Current heap size
    uint32_t memory_used;           // Total memory used by process
    
    // Timing information
    uint32_t creation_time;         // When process was created
    uint32_t cpu_time;              // Total CPU time used
    uint32_t last_run_time;         // Last time process ran
    uint32_t time_slice;            // Time slice for round-robin
    uint32_t remaining_slice;       // Remaining time in current slice
    
    // File descriptors and I/O
    int32_t fd_table[32];           // File descriptor table
    
    // IPC resources
    uint32_t* shared_memory[8];     // Shared memory segments
    int32_t pipes[16];              // Pipe file descriptors
    
    // Process relationships
    struct process* parent;         // Parent process
    struct process* children;       // First child process
    struct process* sibling_next;   // Next sibling
    struct process* sibling_prev;   // Previous sibling
    
    // Scheduler queue pointers
    struct process* next;           // Next in scheduler queue
    struct process* prev;           // Previous in scheduler queue
    
    // Exit status
    int32_t exit_code;              // Exit code when terminated
    
    // Statistics for trading analysis
    uint32_t context_switches;     // Number of context switches
    uint32_t page_faults;          // Number of page faults
    uint32_t syscalls;             // Number of system calls
    uint32_t io_operations;        // Number of I/O operations
} process_t;

// Process statistics
typedef struct process_stats {
    uint32_t total_processes;       // Total processes created
    uint32_t active_processes;      // Currently active processes
    uint32_t running_processes;     // Currently running processes
    uint32_t blocked_processes;     // Blocked processes
    uint32_t context_switches;      // Total context switches
    uint32_t scheduler_ticks;       // Scheduler invocations
    uint32_t load_average;          // System load average
} process_stats_t;

// Scheduler queues
typedef struct scheduler_queue {
    process_t* head;                // First process in queue
    process_t* tail;                // Last process in queue
    uint32_t count;                 // Number of processes in queue
} scheduler_queue_t;

// System call numbers defined in syscalls.h

// Constants
#define MAX_PROCESSES           256
#define DEFAULT_STACK_SIZE      (4 * 1024)     // 4KB default stack
#define DEFAULT_TIME_SLICE      10              // 10ms time slice
#define SCHEDULER_FREQUENCY     100             // 100Hz scheduler
#define IDLE_PROCESS_PID        0
#define INIT_PROCESS_PID        1

// Function prototypes

// Process management core
void process_init(void);
process_t* process_create(const char* name, void* entry_point, process_priority_t priority);
int process_destroy(process_t* process);
process_t* process_find_by_pid(uint32_t pid);
uint32_t process_get_next_pid(void);

// Process lifecycle
int process_fork(process_t* parent);
int process_exec(process_t* process, void* entry_point);
void process_exit(process_t* process, int32_t exit_code);
int process_wait(process_t* parent, uint32_t child_pid, int32_t* exit_code);
int process_kill(uint32_t pid, int32_t signal);

// Scheduler
void scheduler_init(void);
void scheduler_tick(void);              // Called from timer interrupt
process_t* scheduler_pick_next(void);   // Pick next process to run
void scheduler_add_process(process_t* process);
void scheduler_remove_process(process_t* process);
void scheduler_yield(void);             // Voluntary yield
void scheduler_preempt(void);           // Forced preemption

// Context switching
void context_switch(process_t* old_process, process_t* new_process);
void save_context(process_t* process);
void restore_context(process_t* process);

// Process queues
void queue_init(scheduler_queue_t* queue);
void queue_add_tail(scheduler_queue_t* queue, process_t* process);
void queue_add_head(scheduler_queue_t* queue, process_t* process);
process_t* queue_remove_head(scheduler_queue_t* queue);
void queue_remove(scheduler_queue_t* queue, process_t* process);

// System calls
// System call handler defined in syscalls.c

// Process state management
void process_set_state(process_t* process, process_state_t new_state);
void process_block(process_t* process);
void process_unblock(process_t* process);
void process_sleep(process_t* process, uint32_t ms);

// Priority and scheduling
void process_set_priority(process_t* process, process_priority_t priority);
void process_show_all_processes(void);

void process_boost_priority(process_t* process);    // Temporary priority boost
void process_reset_priority(process_t* process);    // Reset to base priority

// Statistics and monitoring
void get_process_stats(process_stats_t* stats);
void print_process_list(void);
void print_scheduler_info(void);
uint32_t get_system_load(void);

// Inter-process communication
int ipc_create_pipe(int32_t* read_fd, int32_t* write_fd);
void* ipc_create_shared_memory(uint32_t size, uint32_t key);
int ipc_destroy_shared_memory(uint32_t key);
void* ipc_attach_shared_memory(process_t* process, uint32_t key);

// Utility functions
uint32_t get_current_time_ms(void);
void process_dump_info(process_t* process);

// Global variables
extern process_t* current_process;      // Currently running process
extern process_t* idle_process;         // Idle process
extern process_stats_t proc_stats;      // Global process statistics
extern scheduler_queue_t ready_queues[5]; // Ready queues for each priority
extern bool scheduler_enabled;          // Scheduler enable flag

#endif // PROCESS_H