#include "../include/types.h"
#include "../include/memory.h"
#include "../include/scheduler.h"

extern "C" void cpp_kernel_main();
extern "C" void handle_timer_interrupt();
extern "C" void handle_network_interrupt();
extern "C" void task_exit();

namespace TradeKernel {

// Global kernel state
bool g_kernel_initialized = false;
cycles_t g_kernel_start_time = 0;

// Console output for early debugging
class EarlyConsole {
private:
    static constexpr u16 VGA_WIDTH = 80;
    static constexpr u16 VGA_HEIGHT = 25;
    static constexpr u8 VGA_COLOR = 0x0F; // White on black
    
    u16* vga_buffer = reinterpret_cast<u16*>(0xB8000);
    u16 cursor_x = 0;
    u16 cursor_y = 0;
    
public:
    void clear() {
        for (u16 y = 0; y < VGA_HEIGHT; y++) {
            for (u16 x = 0; x < VGA_WIDTH; x++) {
                vga_buffer[y * VGA_WIDTH + x] = (VGA_COLOR << 8) | ' ';
            }
        }
        cursor_x = cursor_y = 0;
    }
    
    void print(const char* str) {
        while (*str) {
            if (*str == '\n') {
                cursor_x = 0;
                cursor_y++;
                if (cursor_y >= VGA_HEIGHT) {
                    scroll();
                }
            } else {
                vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = 
                    (VGA_COLOR << 8) | *str;
                cursor_x++;
                if (cursor_x >= VGA_WIDTH) {
                    cursor_x = 0;
                    cursor_y++;
                    if (cursor_y >= VGA_HEIGHT) {
                        scroll();
                    }
                }
            }
            str++;
        }
    }
    
    void print_hex(u64 value) {
        char buffer[17];
        buffer[16] = '\0';
        for (int i = 15; i >= 0; i--) {
            u8 digit = value & 0xF;
            buffer[i] = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
            value >>= 4;
        }
        print("0x");
        print(buffer);
    }
    
private:
    void scroll() {
        for (u16 y = 1; y < VGA_HEIGHT; y++) {
            for (u16 x = 0; x < VGA_WIDTH; x++) {
                vga_buffer[(y-1) * VGA_WIDTH + x] = vga_buffer[y * VGA_WIDTH + x];
            }
        }
        for (u16 x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[(VGA_HEIGHT-1) * VGA_WIDTH + x] = (VGA_COLOR << 8) | ' ';
        }
        cursor_y = VGA_HEIGHT - 1;
    }
};

static EarlyConsole console;

// Performance monitoring
class PerformanceMonitor {
private:
    cycles_t boot_start_cycles;
    cycles_t kernel_init_cycles;
    cycles_t memory_init_cycles;
    cycles_t scheduler_init_cycles;
    
public:
    void record_boot_start() { boot_start_cycles = rdtsc(); }
    void record_kernel_init() { kernel_init_cycles = rdtsc(); }
    void record_memory_init() { memory_init_cycles = rdtsc(); }
    void record_scheduler_init() { scheduler_init_cycles = rdtsc(); }
    
    void print_stats() {
        console.print("Boot Performance Metrics:\n");
        console.print("Kernel Init: ");
        console.print_hex(kernel_init_cycles - boot_start_cycles);
        console.print(" cycles\n");
        console.print("Memory Init: ");
        console.print_hex(memory_init_cycles - kernel_init_cycles);
        console.print(" cycles\n");
        console.print("Scheduler Init: ");
        console.print_hex(scheduler_init_cycles - memory_init_cycles);
        console.print(" cycles\n");
    }
};

static PerformanceMonitor perf_monitor;

// CPU feature detection
class CPUFeatures {
private:
    bool has_rdtsc = false;
    bool has_rdtscp = false;
    bool has_sse = false;
    bool has_sse2 = false;
    bool has_avx = false;
    bool has_avx2 = false;
    u32 cache_line_size = 64;
    
public:
    bool detect_features() {
        u32 eax, ebx, ecx, edx;
        
        // Check for RDTSC
        asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
        has_rdtsc = (edx & (1 << 4)) != 0;
        has_sse = (edx & (1 << 25)) != 0;
        has_sse2 = (edx & (1 << 26)) != 0;
        
        // Check for RDTSCP
        asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000001));
        has_rdtscp = (edx & (1 << 27)) != 0;
        
        // Check for AVX
        asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
        has_avx = (ecx & (1 << 28)) != 0;
        
        // Check for AVX2
        asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(7), "c"(0));
        has_avx2 = (ebx & (1 << 5)) != 0;
        
        return true;
    }
    
    void print_features() {
        console.print("CPU Features:\n");
        console.print("RDTSC: "); console.print(has_rdtsc ? "Yes\n" : "No\n");
        console.print("RDTSCP: "); console.print(has_rdtscp ? "Yes\n" : "No\n");
        console.print("SSE: "); console.print(has_sse ? "Yes\n" : "No\n");
        console.print("SSE2: "); console.print(has_sse2 ? "Yes\n" : "No\n");
        console.print("AVX: "); console.print(has_avx ? "Yes\n" : "No\n");
        console.print("AVX2: "); console.print(has_avx2 ? "Yes\n" : "No\n");
    }
};

static CPUFeatures cpu_features;

// Test task functions
void market_data_task(void* arg) {
    console.print("Market Data Task Started\n");
    
    for (int i = 0; i < 1000; i++) {
        // Simulate market data processing
        cycles_t start = rdtsc();
        
        // Simulate some work
        volatile int sum = 0;
        for (int j = 0; j < 1000; j++) {
            sum += j;
        }
        
        cycles_t end = rdtsc();
        
        if (i % 100 == 0) {
            console.print("Market data processed, cycles: ");
            console.print_hex(end - start);
            console.print("\n");
        }
        
        // Yield to other tasks
        Scheduler::scheduler_yield();
    }
    
    console.print("Market Data Task Finished\n");
}

void order_execution_task(void* arg) {
    console.print("Order Execution Task Started\n");
    
    for (int i = 0; i < 500; i++) {
        // Simulate order execution
        cycles_t start = rdtsc();
        
        // Simulate order processing work
        volatile int result = 0;
        for (int j = 0; j < 2000; j++) {
            result += j * j;
        }
        
        cycles_t end = rdtsc();
        
        if (i % 50 == 0) {
            console.print("Order executed, cycles: ");
            console.print_hex(end - start);
            console.print("\n");
        }
        
        // Yield to other tasks
        Scheduler::scheduler_yield();
    }
    
    console.print("Order Execution Task Finished\n");
}

void risk_management_task(void* arg) {
    console.print("Risk Management Task Started\n");
    
    for (int i = 0; i < 200; i++) {
        // Simulate risk calculations
        cycles_t start = rdtsc();
        
        // Simulate risk calculation work
        volatile double risk = 0.0;
        for (int j = 0; j < 1000; j++) {
            risk += j * 0.001;
        }
        
        cycles_t end = rdtsc();
        
        if (i % 20 == 0) {
            console.print("Risk calculated, cycles: ");
            console.print_hex(end - start);
            console.print("\n");
        }
        
        // Yield to other tasks
        Scheduler::scheduler_yield();
    }
    
    console.print("Risk Management Task Finished\n");
}

// Kernel initialization
bool initialize_kernel() {
    perf_monitor.record_boot_start();
    
    console.clear();
    console.print("TradeKernel v1.0 - Ultra-Low Latency Trading OS\n");
    console.print("================================================\n\n");
    
    perf_monitor.record_kernel_init();
    
    // Detect CPU features
    console.print("Detecting CPU features...\n");
    if (!cpu_features.detect_features()) {
        console.print("ERROR: Failed to detect CPU features\n");
        return false;
    }
    cpu_features.print_features();
    console.print("\n");
    
    // Initialize memory subsystem
    console.print("Initializing memory subsystem...\n");
    perf_monitor.record_memory_init();
    if (!Memory::initialize_memory_subsystem()) {
        console.print("ERROR: Failed to initialize memory subsystem\n");
        return false;
    }
    console.print("Memory subsystem initialized\n\n");
    
    // Initialize scheduler
    console.print("Initializing scheduler...\n");
    perf_monitor.record_scheduler_init();
    if (!Scheduler::initialize_scheduler()) {
        console.print("ERROR: Failed to initialize scheduler\n");
        return false;
    }
    console.print("Scheduler initialized\n\n");
    
    // Print performance statistics
    perf_monitor.print_stats();
    console.print("\n");
    
    g_kernel_start_time = rdtsc();
    g_kernel_initialized = true;
    
    return true;
}

// Create and start test tasks
void start_trading_tasks() {
    console.print("Starting trading tasks...\n");
    
    // Create market data task (highest priority)
    u32 market_task = Scheduler::g_scheduler->create_task(
        Priority::CRITICAL, 
        market_data_task, 
        nullptr,
        16384  // 16KB stack
    );
    
    // Create order execution task (high priority)
    u32 order_task = Scheduler::g_scheduler->create_task(
        Priority::HIGH,
        order_execution_task,
        nullptr,
        16384  // 16KB stack
    );
    
    // Create risk management task (normal priority)
    u32 risk_task = Scheduler::g_scheduler->create_task(
        Priority::NORMAL,
        risk_management_task,
        nullptr,
        16384  // 16KB stack
    );
    
    console.print("Tasks created - Market: ");
    console.print_hex(market_task);
    console.print(", Order: ");
    console.print_hex(order_task);
    console.print(", Risk: ");
    console.print_hex(risk_task);
    console.print("\n\n");
    
    console.print("Beginning task execution...\n");
}

} // namespace TradeKernel

// C interface functions
extern "C" void cpp_kernel_main() {
    using namespace TradeKernel;
    
    if (!initialize_kernel()) {
        console.print("FATAL: Kernel initialization failed\n");
        while (true) {
            asm volatile("hlt");
        }
    }
    
    console.print("Kernel initialization complete!\n\n");
    
    // Start trading tasks
    start_trading_tasks();
    
    // Enter scheduler main loop
    console.print("Entering scheduler main loop...\n");
    while (true) {
        Scheduler::g_scheduler->schedule_next();
        asm volatile("hlt"); // Wait for interrupts
    }
}

extern "C" void handle_timer_interrupt() {
    // Handle timer interrupt for scheduling
    if (TradeKernel::g_kernel_initialized && Scheduler::g_scheduler) {
        // Get current CPU core and handle timer
        // This would call the scheduler's timer handler
    }
}

extern "C" void handle_network_interrupt() {
    // Handle network interrupts for ultra-low latency packet processing
    if (TradeKernel::g_kernel_initialized) {
        // Process network packets directly in interrupt context
        // for minimum latency
    }
}

extern "C" void task_exit() {
    // Called when a task finishes execution
    if (TradeKernel::g_kernel_initialized && Scheduler::g_scheduler) {
        u32 task_id = Scheduler::get_current_task_id();
        Scheduler::g_scheduler->destroy_task(task_id);
        Scheduler::scheduler_yield();
    }
}
