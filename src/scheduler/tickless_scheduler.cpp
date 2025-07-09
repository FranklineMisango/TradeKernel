#include "../include/scheduler.h"
#include "../include/memory.h"

extern "C" {
    void context_switch_asm(TradeKernel::Scheduler::CPUContext* from, 
                           TradeKernel::Scheduler::CPUContext* to);
    u64 get_cpu_id();
}

namespace TradeKernel {
namespace Scheduler {

// Global scheduler instance
TicklessScheduler* g_scheduler = nullptr;

// Task implementation
Task::Task(u32 id, Priority prio, void (*func)(void*), void* argument, 
           size_t stack_sz, u64 affinity)
    : task_id(id)
    , priority(prio)
    , state(TaskState::READY)
    , creation_time(rdtsc())
    , last_run_time(0)
    , total_runtime(0)
    , deadline(0)
    , stack_base(nullptr)
    , stack_size(stack_sz)
    , next(nullptr)
    , prev(nullptr)
    , cpu_affinity(affinity)
    , entry_point(func)
    , arg(argument) {
    
    if (!allocate_stack()) {
        state = TaskState::TERMINATED;
        return;
    }
    
    setup_initial_context();
}

Task::~Task() {
    if (stack_base) {
        Memory::fast_free(stack_base);
    }
}

bool Task::allocate_stack() {
    stack_base = Memory::fast_alloc(stack_size + 4096); // Extra page for guard
    if (!stack_base) {
        return false;
    }
    
    // Set up stack guard page (would use mprotect in real implementation)
    // mprotect(stack_base, 4096, PROT_NONE);
    
    return true;
}

void Task::setup_initial_context() {
    // Clear the context
    for (size_t i = 0; i < sizeof(CPUContext); i++) {
        reinterpret_cast<u8*>(&context)[i] = 0;
    }
    
    // Set up stack pointer (grows downward)
    context.rsp = reinterpret_cast<u64>(stack_base) + stack_size - 8;
    context.rbp = context.rsp;
    
    // Set up entry point
    context.rip = reinterpret_cast<u64>(entry_point);
    
    // Set up initial argument in RDI
    context.rdi = reinterpret_cast<u64>(arg);
    
    // Set up flags (enable interrupts)
    context.rflags = 0x200;
    
    // Initialize FPU state
    // In real implementation, would use fxsave/fxrstor
}

void Task::execute() {
    if (state != TaskState::READY && state != TaskState::RUNNING) {
        return;
    }
    
    state = TaskState::RUNNING;
    last_run_time = rdtsc();
    
    // Execute task function
    if (entry_point) {
        entry_point(arg);
    }
    
    state = TaskState::TERMINATED;
}

void Task::yield() {
    if (state == TaskState::RUNNING) {
        state = TaskState::READY;
    }
}

void Task::terminate() {
    state = TaskState::TERMINATED;
}

// Priority Queue implementation
PriorityQueue::PriorityQueue() : bitmap(0) {
    for (size_t i = 0; i < NUM_PRIORITIES; i++) {
        heads[i] = nullptr;
        tails[i] = nullptr;
    }
}

PriorityQueue::~PriorityQueue() {
    // Tasks are managed by the scheduler, not the queue
}

void PriorityQueue::enqueue(Task* task) {
    if (!task) return;
    
    u32 priority_level = static_cast<u32>(task->get_priority());
    if (priority_level >= NUM_PRIORITIES) {
        priority_level = NUM_PRIORITIES - 1;
    }
    
    insert_task(task, priority_level);
}

Task* PriorityQueue::dequeue() {
    return remove_highest_priority_task();
}

Task* PriorityQueue::peek() const {
    if (bitmap == 0) return nullptr;
    
    // Find highest priority non-empty queue
    for (u32 i = 0; i < NUM_PRIORITIES; i++) {
        if ((bitmap & (1 << i)) && heads[i]) {
            return heads[i];
        }
    }
    
    return nullptr;
}

void PriorityQueue::insert_task(Task* task, u32 priority_level) {
    task->set_next(nullptr);
    task->set_prev(tails[priority_level]);
    
    if (tails[priority_level]) {
        tails[priority_level]->set_next(task);
    } else {
        heads[priority_level] = task;
    }
    
    tails[priority_level] = task;
    bitmap |= (1 << priority_level);
}

Task* PriorityQueue::remove_highest_priority_task() {
    if (bitmap == 0) return nullptr;
    
    // Find highest priority non-empty queue
    for (u32 i = 0; i < NUM_PRIORITIES; i++) {
        if ((bitmap & (1 << i)) && heads[i]) {
            Task* task = heads[i];
            
            heads[i] = task->get_next();
            if (heads[i]) {
                heads[i]->set_prev(nullptr);
            } else {
                tails[i] = nullptr;
                bitmap &= ~(1 << i);
            }
            
            task->set_next(nullptr);
            task->set_prev(nullptr);
            
            return task;
        }
    }
    
    return nullptr;
}

// CPU Core implementation
CPUCore::CPUCore(u32 id) 
    : core_id(id)
    , current_task(nullptr)
    , idle_task(nullptr)
    , last_context_switch(0)
    , context_switch_count(0)
    , avg_context_switch_time(0)
    , task_count(0)
    , total_load(0) {
}

CPUCore::~CPUCore() {
    delete idle_task;
}

void CPUCore::initialize() {
    create_idle_task();
    current_task = idle_task;
}

void CPUCore::schedule() {
    cycles_t start_time = rdtsc();
    
    Task* next_task = ready_queue.dequeue();
    if (!next_task) {
        next_task = idle_task;
    }
    
    if (next_task != current_task) {
        context_switch(current_task, next_task);
    }
    
    cycles_t end_time = rdtsc();
    
    // Update statistics
    context_switch_count++;
    nanoseconds_t switch_time = end_time - start_time;
    avg_context_switch_time = (avg_context_switch_time + switch_time) / 2;
    last_context_switch = end_time;
}

void CPUCore::add_task(Task* task) {
    if (!task) return;
    
    ready_queue.enqueue(task);
    task_count++;
    update_load_statistics();
}

void CPUCore::remove_task(Task* task) {
    if (!task) return;
    
    // Remove from ready queue (simplified)
    if (task_count > 0) {
        task_count--;
    }
    update_load_statistics();
}

void CPUCore::context_switch(Task* from, Task* to) {
    if (!to) return;
    
    cycles_t switch_start = rdtsc();
    
    if (from && from != idle_task) {
        from->set_state(TaskState::READY);
        // Add back to ready queue if not terminated
        if (from->get_state() != TaskState::TERMINATED) {
            ready_queue.enqueue(from);
        }
    }
    
    current_task = to;
    to->set_state(TaskState::RUNNING);
    
    // Perform actual context switch
    if (from && from != to) {
        context_switch_asm(&from->get_context(), &to->get_context());
    }
    
    cycles_t switch_end = rdtsc();
    
    // Update task runtime
    if (from) {
        cycles_t runtime = switch_start - from->last_run_time;
        // from->total_runtime += runtime; // Would need atomic update
    }
    
    to->last_run_time = switch_end;
}

void CPUCore::handle_timer_interrupt() {
    // Force scheduling on timer interrupt
    schedule();
}

void CPUCore::create_idle_task() {
    auto idle_func = [](void*) {
        while (true) {
            asm volatile("hlt"); // Wait for interrupts
        }
    };
    
    idle_task = new Task(0, Priority::IDLE, 
                        reinterpret_cast<void(*)(void*)>(idle_func), 
                        nullptr, 4096);
}

void CPUCore::update_load_statistics() {
    // Simple load calculation based on task count
    total_load = task_count * 100; // Simplified metric
}

// Tickless Scheduler implementation
TicklessScheduler::TicklessScheduler() 
    : num_cores(0)
    , next_task_id(1)
    , last_balance_time(0) {
    
    for (size_t i = 0; i < MAX_CPUS; i++) {
        cpu_cores[i] = nullptr;
    }
    
    for (size_t i = 0; i < 4096; i++) {
        task_table[i] = nullptr;
    }
}

TicklessScheduler::~TicklessScheduler() {
    shutdown();
}

bool TicklessScheduler::initialize() {
    detect_cpu_topology();
    setup_per_cpu_data();
    
    return true;
}

void TicklessScheduler::shutdown() {
    for (u32 i = 0; i < num_cores; i++) {
        delete cpu_cores[i];
        cpu_cores[i] = nullptr;
    }
    
    for (size_t i = 0; i < 4096; i++) {
        delete task_table[i];
        task_table[i] = nullptr;
    }
}

u32 TicklessScheduler::create_task(Priority priority, void (*func)(void*), 
                                   void* arg, size_t stack_size, u64 cpu_affinity) {
    u32 task_id = __sync_fetch_and_add(&next_task_id, 1);
    
    if (task_id >= 4096) {
        return 0; // Task table full
    }
    
    Task* task = new Task(task_id, priority, func, arg, stack_size, cpu_affinity);
    if (!task || task->get_state() == TaskState::TERMINATED) {
        delete task;
        return 0;
    }
    
    task_table[task_id] = task;
    
    // Assign to least loaded CPU
    CPUCore* core = get_least_loaded_core();
    if (core) {
        core->add_task(task);
    }
    
    return task_id;
}

bool TicklessScheduler::destroy_task(u32 task_id) {
    if (task_id >= 4096 || !task_table[task_id]) {
        return false;
    }
    
    Task* task = task_table[task_id];
    task->terminate();
    
    // Remove from CPU core
    for (u32 i = 0; i < num_cores; i++) {
        if (cpu_cores[i]) {
            cpu_cores[i]->remove_task(task);
        }
    }
    
    delete task;
    task_table[task_id] = nullptr;
    
    return true;
}

Task* TicklessScheduler::get_task(u32 task_id) {
    if (task_id >= 4096) {
        return nullptr;
    }
    return task_table[task_id];
}

void TicklessScheduler::yield() {
    u32 cpu_id = get_current_cpu_id();
    if (cpu_id < num_cores && cpu_cores[cpu_id]) {
        cpu_cores[cpu_id]->schedule();
    }
}

void TicklessScheduler::sleep(nanoseconds_t ns) {
    // Simplified sleep - just yield for now
    // In real implementation, would set up timer
    yield();
}

void TicklessScheduler::schedule_next() {
    cycles_t current_time = rdtsc();
    
    // Check if load balancing is needed
    if (current_time - last_balance_time > BALANCE_INTERVAL) {
        balance_load();
        last_balance_time = current_time;
    }
    
    // Schedule on current CPU
    u32 cpu_id = get_current_cpu_id();
    if (cpu_id < num_cores && cpu_cores[cpu_id]) {
        cpu_cores[cpu_id]->schedule();
    }
}

void TicklessScheduler::balance_load() {
    // Simple load balancing - move tasks from overloaded to underloaded cores
    // This is a simplified version
    
    if (num_cores < 2) return;
    
    CPUCore* min_core = cpu_cores[0];
    CPUCore* max_core = cpu_cores[0];
    
    for (u32 i = 1; i < num_cores; i++) {
        if (cpu_cores[i]->get_load() < min_core->get_load()) {
            min_core = cpu_cores[i];
        }
        if (cpu_cores[i]->get_load() > max_core->get_load()) {
            max_core = cpu_cores[i];
        }
    }
    
    // If load difference is significant, balance
    if (max_core->get_load() - min_core->get_load() > 200) {
        // Would implement task migration here
    }
}

CPUCore* TicklessScheduler::get_least_loaded_core() {
    if (num_cores == 0) return nullptr;
    
    CPUCore* least_loaded = cpu_cores[0];
    for (u32 i = 1; i < num_cores; i++) {
        if (cpu_cores[i]->get_load() < least_loaded->get_load()) {
            least_loaded = cpu_cores[i];
        }
    }
    
    return least_loaded;
}

void TicklessScheduler::detect_cpu_topology() {
    // Simplified CPU detection
    num_cores = 4; // Assume 4 cores for now
    if (num_cores > MAX_CPUS) {
        num_cores = MAX_CPUS;
    }
}

void TicklessScheduler::setup_per_cpu_data() {
    for (u32 i = 0; i < num_cores; i++) {
        cpu_cores[i] = new CPUCore(i);
        cpu_cores[i]->initialize();
    }
}

u32 TicklessScheduler::get_current_cpu_id() {
    return static_cast<u32>(get_cpu_id()) % num_cores;
}

TicklessScheduler::SchedulerStats TicklessScheduler::get_stats() const {
    SchedulerStats stats = {};
    
    for (u32 i = 0; i < num_cores; i++) {
        if (cpu_cores[i]) {
            stats.total_context_switches += cpu_cores[i]->context_switch_count;
            // Would aggregate other stats
        }
    }
    
    // Count active tasks
    for (size_t i = 0; i < 4096; i++) {
        if (task_table[i] && task_table[i]->get_state() != TaskState::TERMINATED) {
            stats.active_tasks++;
        }
        if (task_table[i]) {
            stats.total_tasks_created++;
        }
    }
    
    return stats;
}

// Global functions
bool initialize_scheduler() {
    if (g_scheduler) {
        return false; // Already initialized
    }
    
    g_scheduler = new TicklessScheduler();
    if (!g_scheduler) {
        return false;
    }
    
    return g_scheduler->initialize();
}

void shutdown_scheduler() {
    delete g_scheduler;
    g_scheduler = nullptr;
}

} // namespace Scheduler
} // namespace TradeKernel
