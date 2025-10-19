#include "process.h"
#include "../mm/memory.h"
#include "../drivers/vga.h"

// External variables
extern process_t* current_process;
extern process_t* idle_process;
extern process_stats_t proc_stats;
extern scheduler_queue_t ready_queues[5];
extern bool scheduler_enabled;

// Initialize scheduler
void scheduler_init(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("Initializing scheduler...\n");
    
    scheduler_enabled = true;
    
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write_string("Priority-based scheduler initialized\n");
}

// Main scheduler tick - called from timer interrupt
void scheduler_tick(void) {
    if (!scheduler_enabled || !current_process) {
        return;
    }
    
    // Update current process CPU time
    current_process->cpu_time++;
    
    // Decrease remaining time slice for round-robin processes
    if (current_process->policy == SCHED_RR && current_process->remaining_slice > 0) {
        current_process->remaining_slice--;
    }
    
    // Check if current process should be preempted
    bool should_preempt = false;
    
    // Real-time processes run until completion or blocking
    if (current_process->policy == SCHED_FIFO) {
        // Check if higher priority process is ready
        for (int i = 0; i < (int)current_process->priority; i++) {
            if (ready_queues[i].count > 0) {
                should_preempt = true;
                break;
            }
        }
    }
    // Round-robin processes are preempted when time slice expires
    else if (current_process->policy == SCHED_RR) {
        if (current_process->remaining_slice == 0) {
            should_preempt = true;
        }
        // Also check for higher priority processes
        for (int i = 0; i < (int)current_process->priority; i++) {
            if (ready_queues[i].count > 0) {
                should_preempt = true;
                break;
            }
        }
    }
    
    if (should_preempt) {
        scheduler_preempt();
    }
}

// Pick next process to run using priority-based scheduling
process_t* scheduler_pick_next(void) {
    // First check real-time and high priority processes
    for (int priority = PRIORITY_REALTIME; priority <= PRIORITY_IDLE; priority++) {
        if (ready_queues[priority].count > 0) {
            process_t* next = queue_remove_head(&ready_queues[priority]);
            if (next) {
                return next;
            }
        }
    }
    
    // If no processes are ready, return idle process
    return idle_process;
}

// Add process to appropriate ready queue
void scheduler_add_process(process_t* process) {
    if (!process || process->state != PROCESS_READY) {
        return;
    }
    
    // Reset time slice for round-robin processes
    if (process->policy == SCHED_RR) {
        process->remaining_slice = process->time_slice;
    }
    
    // Add to priority queue
    queue_add_tail(&ready_queues[process->priority], process);
}

// Remove process from scheduler queues
void scheduler_remove_process(process_t* process) {
    if (!process) return;
    
    // Remove from ready queues
    for (int i = 0; i < 5; i++) {
        queue_remove(&ready_queues[i], process);
    }
}

// Voluntary yield - process gives up CPU
void scheduler_yield(void) {
    if (!scheduler_enabled || !current_process) {
        return;
    }
    
    process_t* old_process = current_process;
    
    // If current process is still ready, add it back to queue
    if (old_process->state == PROCESS_RUNNING) {
        process_set_state(old_process, PROCESS_READY);
    }
    
    // Pick next process
    process_t* next_process = scheduler_pick_next();
    if (next_process && next_process != old_process) {
        process_set_state(next_process, PROCESS_RUNNING);
        current_process = next_process;
        next_process->last_run_time = get_current_time_ms();
        
        // Perform context switch
        context_switch(old_process, next_process);
        
        // Update statistics
        proc_stats.context_switches++;
        old_process->context_switches++;
        next_process->context_switches++;
    }
}

// Forced preemption - scheduler forces context switch
void scheduler_preempt(void) {
    if (!scheduler_enabled || !current_process) {
        return;
    }
    
    process_t* old_process = current_process;
    
    // Move current process back to ready queue if still runnable
    if (old_process->state == PROCESS_RUNNING) {
        process_set_state(old_process, PROCESS_READY);
    }
    
    // Pick next process
    process_t* next_process = scheduler_pick_next();
    if (next_process) {
        process_set_state(next_process, PROCESS_RUNNING);
        current_process = next_process;
        next_process->last_run_time = get_current_time_ms();
        
        // Perform context switch
        context_switch(old_process, next_process);
        
        // Update statistics
        proc_stats.context_switches++;
        old_process->context_switches++;
        next_process->context_switches++;
    }
}

// Print scheduler information
void print_scheduler_info(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("=== Scheduler Information ===\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    vga_write_string("Current process: ");
    if (current_process) {
        vga_write_string(current_process->name);
        vga_write_string(" (PID ");
        print_number(current_process->pid);
        vga_write_string(")\n");
    } else {
        vga_write_string("None\n");
    }
    
    vga_write_string("Ready queue counts:\n");
    vga_write_string("  Real-time: ");
    print_number(ready_queues[PRIORITY_REALTIME].count);
    vga_write_string("\n  High:      ");
    print_number(ready_queues[PRIORITY_HIGH].count);
    vga_write_string("\n  Normal:    ");
    print_number(ready_queues[PRIORITY_NORMAL].count);
    vga_write_string("\n  Low:       ");
    print_number(ready_queues[PRIORITY_LOW].count);
    vga_write_string("\n  Idle:      ");
    print_number(ready_queues[PRIORITY_IDLE].count);
    vga_write_string("\n");
    
    vga_write_string("Total context switches: ");
    print_number(proc_stats.context_switches);
    vga_write_string("\n");
    
    vga_write_string("System load: ");
    print_number(proc_stats.load_average / 100);
    vga_write_string(".");
    print_number(proc_stats.load_average % 100);
    vga_write_string(">\n\n");
}

// Get system load average
uint32_t get_system_load(void) {
    return proc_stats.load_average;
}

// Queue management functions
void queue_init(scheduler_queue_t* queue) {
    if (!queue) return;
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
}

void queue_add_tail(scheduler_queue_t* queue, process_t* process) {
    if (!queue || !process) return;
    
    process->next = NULL;
    process->prev = queue->tail;
    
    if (queue->tail) {
        queue->tail->next = process;
    } else {
        queue->head = process;
    }
    
    queue->tail = process;
    queue->count++;
}

void queue_add_head(scheduler_queue_t* queue, process_t* process) {
    if (!queue || !process) return;
    
    process->prev = NULL;
    process->next = queue->head;
    
    if (queue->head) {
        queue->head->prev = process;
    } else {
        queue->tail = process;
    }
    
    queue->head = process;
    queue->count++;
}

process_t* queue_remove_head(scheduler_queue_t* queue) {
    if (!queue || !queue->head) return NULL;
    
    process_t* process = queue->head;
    queue->head = process->next;
    
    if (queue->head) {
        queue->head->prev = NULL;
    } else {
        queue->tail = NULL;
    }
    
    process->next = NULL;
    process->prev = NULL;
    queue->count--;
    
    return process;
}

void queue_remove(scheduler_queue_t* queue, process_t* process) {
    if (!queue || !process) return;
    
    if (process->prev) {
        process->prev->next = process->next;
    } else {
        queue->head = process->next;
    }
    
    if (process->next) {
        process->next->prev = process->prev;
    } else {
        queue->tail = process->prev;
    }
    
    process->next = NULL;
    process->prev = NULL;
    queue->count--;
}


void scheduler_show_stats(void) {
    vga_write_string("Scheduler Statistics:\n");
    vga_write_string("Total context switches: ");
    print_dec(proc_stats.context_switches);
    vga_write_string("\n");
    vga_write_string("System load: ");
    print_dec(proc_stats.load_average / 100);
    vga_write_string(".");
    print_dec(proc_stats.load_average % 100);
    vga_write_string("\n");
    vga_write_string("N/A\n");
    vga_write_string("Active processes per priority:\n");
    
    for (int i = 0; i < 5; i++) {
        vga_write_string("Priority ");
        print_dec(i);
        vga_write_string(": ");
        print_dec(ready_queues[i].count);
        vga_write_string(" processes\n");
    }
}