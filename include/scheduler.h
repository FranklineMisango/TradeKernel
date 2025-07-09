#ifndef TRADEKERNEL_SCHEDULER_H
#define TRADEKERNEL_SCHEDULER_H

#include "types.h"
#include "memory.h"
#include <array>

namespace TradeKernel {
namespace Scheduler {

// Forward declarations
class Task;
class CPUCore;

// CPU context for task switching
struct PACKED CPUContext {
    // General purpose registers
    u64 rax, rbx, rcx, rdx;
    u64 rsi, rdi, rbp, rsp;
    u64 r8, r9, r10, r11;
    u64 r12, r13, r14, r15;
    
    // Segment registers
    u64 cs, ds, es, fs, gs, ss;
    
    // Control registers
    u64 rip, rflags;
    
    // FPU/SSE state (512 bytes for FXSAVE/FXRSTOR)
    u8 fpu_state[512] CACHE_ALIGNED;
} CACHE_ALIGNED;

// Task Control Block
class CACHE_ALIGNED Task {
private:
    u32 task_id;
    Priority priority;
    TaskState state;
    CPUContext context;
    
    // Timing information
    cycles_t creation_time;
    cycles_t last_run_time;
    cycles_t total_runtime;
    cycles_t deadline;
    
    // Stack information
    void* stack_base;
    size_t stack_size;
    
    // Linked list pointers for scheduler queues
    Task* next;
    Task* prev;
    
    // CPU affinity mask
    u64 cpu_affinity;
    
    // Task function and arguments
    void (*entry_point)(void*);
    void* arg;
    
public:
    Task(u32 id, Priority prio, void (*func)(void*), void* argument, 
         size_t stack_sz = 8192, u64 affinity = UINT64_MAX);
    ~Task();
    
    // Getters
    u32 get_id() const { return task_id; }
    Priority get_priority() const { return priority; }
    TaskState get_state() const { return state; }
    CPUContext& get_context() { return context; }
    cycles_t get_creation_time() const { return creation_time; }
    cycles_t get_total_runtime() const { return total_runtime; }
    u64 get_cpu_affinity() const { return cpu_affinity; }
    
    // State management
    void set_state(TaskState new_state) { state = new_state; }
    void set_priority(Priority new_priority) { priority = new_priority; }
    void set_deadline(cycles_t deadline_cycles) { deadline = deadline_cycles; }
    
    // For linked list management
    Task* get_next() const { return next; }
    Task* get_prev() const { return prev; }
    void set_next(Task* task) { next = task; }
    void set_prev(Task* task) { prev = task; }
    
    // Execute the task
    void execute();
    void yield();
    void terminate();
    
private:
    bool allocate_stack();
    void setup_initial_context();
};

// Lock-free priority queue for tasks
class CACHE_ALIGNED PriorityQueue {
private:
    static constexpr size_t NUM_PRIORITIES = 5;
    Task* heads[NUM_PRIORITIES];
    Task* tails[NUM_PRIORITIES];
    u32 bitmap; // Bitmask for non-empty queues
    
public:
    PriorityQueue();
    ~PriorityQueue();
    
    void enqueue(Task* task);
    Task* dequeue();
    Task* peek() const;
    bool empty() const { return bitmap == 0; }
    
private:
    void insert_task(Task* task, u32 priority_level);
    Task* remove_highest_priority_task();
};

// Per-CPU scheduler state
class CACHE_ALIGNED CPUCore {
private:
    u32 core_id;
    Task* current_task;
    Task* idle_task;
    PriorityQueue ready_queue;
    
    // Scheduler statistics
    cycles_t last_context_switch;
    u64 context_switch_count;
    nanoseconds_t avg_context_switch_time;
    
    // Load balancing
    u32 task_count;
    u64 total_load;
    
public:
    explicit CPUCore(u32 id);
    ~CPUCore();
    
    void initialize();
    void schedule();
    void add_task(Task* task);
    void remove_task(Task* task);
    
    Task* get_current_task() const { return current_task; }
    u32 get_core_id() const { return core_id; }
    u32 get_task_count() const { return task_count; }
    u64 get_load() const { return total_load; }
    
    void context_switch(Task* from, Task* to);
    void handle_timer_interrupt();
    
private:
    void create_idle_task();
    void update_load_statistics();
};

// Global scheduler manager
class TicklessScheduler {
private:
    static constexpr size_t MAX_CPUS = 64;
    
    std::array<CPUCore*, MAX_CPUS> cpu_cores;
    u32 num_cores;
    Task* task_table[4096]; // Max 4096 tasks
    u32 next_task_id;
    
    // Load balancing
    cycles_t last_balance_time;
    static constexpr cycles_t BALANCE_INTERVAL = 1000000; // 1M cycles
    
public:
    TicklessScheduler();
    ~TicklessScheduler();
    
    bool initialize();
    void shutdown();
    
    // Task management
    u32 create_task(Priority priority, void (*func)(void*), void* arg, 
                    size_t stack_size = 8192, u64 cpu_affinity = UINT64_MAX);
    bool destroy_task(u32 task_id);
    Task* get_task(u32 task_id);
    
    // Scheduling operations
    void yield();
    void sleep(nanoseconds_t ns);
    void schedule_next();
    
    // Load balancing
    void balance_load();
    CPUCore* get_least_loaded_core();
    
    // Statistics
    struct SchedulerStats {
        u64 total_context_switches;
        nanoseconds_t avg_context_switch_time;
        nanoseconds_t max_context_switch_time;
        u32 active_tasks;
        u32 total_tasks_created;
    };
    
    SchedulerStats get_stats() const;
    
private:
    void detect_cpu_topology();
    void setup_per_cpu_data();
    u32 get_current_cpu_id();
};

// Assembly functions for context switching
extern "C" {
    void context_switch_asm(CPUContext* from, CPUContext* to);
    void task_entry_point_asm();
    u64 get_cpu_id();
    void set_task_stack(void* stack_top);
}

// Global scheduler instance
extern TicklessScheduler* g_scheduler;

// Initialization
bool initialize_scheduler();
void shutdown_scheduler();

// Utility functions
FORCE_INLINE u32 get_current_task_id() {
    CPUCore* core = g_scheduler->get_least_loaded_core(); // Temporary
    Task* task = core->get_current_task();
    return task ? task->get_id() : 0;
}

FORCE_INLINE void scheduler_yield() {
    g_scheduler->yield();
}

FORCE_INLINE void scheduler_sleep(nanoseconds_t ns) {
    g_scheduler->sleep(ns);
}

} // namespace Scheduler
} // namespace TradeKernel

#endif // TRADEKERNEL_SCHEDULER_H
