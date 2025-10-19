#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "../types.h"
#include "process.h"

// Scheduling policies
#define SCHED_FIFO      0   // First In, First Out (non-preemptive within priority)
#define SCHED_RR        1   // Round Robin (preemptive)
#define SCHED_CFS       2   // Completely Fair Scheduler (not implemented)

// Time slice configuration
#define DEFAULT_TIME_SLICE  10  // 10 timer ticks
#define MIN_TIME_SLICE      1   
#define MAX_TIME_SLICE      100

// Load balancing
#define LOAD_BALANCE_INTERVAL   100  // Timer ticks between load balancing

// Scheduler statistics
typedef struct {
    uint32_t total_switches;
    uint32_t preemptions;
    uint32_t idle_time;
    uint32_t load_balance_runs;
    uint32_t queue_lengths[5]; // NUM_PRIORITY_LEVELS from process.h
} scheduler_stats_t;

// Use scheduler_queue_t from process.h

// Scheduler functions
void scheduler_init(void);
void scheduler_tick(void);
void scheduler_add_process(process_t* process);
void scheduler_remove_process(process_t* process);
void scheduler_yield(void);
void scheduler_schedule(void);
void scheduler_enable(void);
void scheduler_disable(void);
process_t* scheduler_pick_next(void);
void scheduler_update_load_average(void);
void scheduler_show_stats(void);

// Queue management functions are in process.h

// Load balancing functions
void balance_queues(void);
void migrate_process(process_t* process, int new_priority);

// Real-time scheduling support
void scheduler_set_realtime_mode(int enabled);
void scheduler_boost_priority(process_t* process);
void scheduler_decay_priority(process_t* process);

// Trading-specific scheduling functions
void scheduler_set_trading_mode(int enabled);
void scheduler_prioritize_market_data(void);
void scheduler_handle_order_urgency(process_t* process);

#endif