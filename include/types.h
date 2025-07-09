#ifndef TRADEKERNEL_TYPES_H
#define TRADEKERNEL_TYPES_H

#include <cstdint>

namespace TradeKernel {

// Core types for ultra-low latency operations
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using size_t = u64;
using uintptr_t = u64;
using ptrdiff_t = i64;

// High-precision timing types
using cycles_t = u64;
using nanoseconds_t = u64;
using microseconds_t = u64;

// Memory alignment macros
#define CACHE_LINE_SIZE 64
#define CACHE_ALIGNED __attribute__((aligned(CACHE_LINE_SIZE)))
#define PACKED __attribute__((packed))
#define FORCE_INLINE __attribute__((always_inline)) inline
#define NO_INLINE __attribute__((noinline))

// CPU optimization hints
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define PREFETCH_READ(addr) __builtin_prefetch(addr, 0, 3)
#define PREFETCH_WRITE(addr) __builtin_prefetch(addr, 1, 3)

// Memory barriers
#define COMPILER_BARRIER() asm volatile("" ::: "memory")
#define MEMORY_BARRIER() asm volatile("mfence" ::: "memory")
#define READ_BARRIER() asm volatile("lfence" ::: "memory")
#define WRITE_BARRIER() asm volatile("sfence" ::: "memory")

// CPU pause for spin locks
#define CPU_PAUSE() asm volatile("pause" ::: "memory")

// RDTSC for cycle counting
FORCE_INLINE cycles_t rdtsc() {
    u32 lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((u64)hi << 32) | lo;
}

FORCE_INLINE cycles_t rdtscp() {
    u32 lo, hi;
    asm volatile("rdtscp" : "=a"(lo), "=d"(hi) :: "ecx");
    return ((u64)hi << 32) | lo;
}

// Priority levels for scheduler
enum class Priority : u8 {
    CRITICAL = 0,    // Market data processing
    HIGH = 1,        // Order execution
    NORMAL = 2,      // Risk management
    LOW = 3,         // Logging/admin
    IDLE = 4         // Background tasks
};

// Task states
enum class TaskState : u8 {
    READY = 0,
    RUNNING = 1,
    BLOCKED = 2,
    TERMINATED = 3
};

// Network packet types
enum class PacketType : u8 {
    MARKET_DATA = 0,
    ORDER = 1,
    EXECUTION = 2,
    HEARTBEAT = 3,
    ADMIN = 4
};

// Error codes
enum class ErrorCode : i32 {
    SUCCESS = 0,
    INVALID_PARAMETER = -1,
    OUT_OF_MEMORY = -2,
    TIMEOUT = -3,
    HARDWARE_ERROR = -4,
    NETWORK_ERROR = -5,
    PERMISSION_DENIED = -6
};

// Result type for error handling
template<typename T>
struct Result {
    T value;
    ErrorCode error;
    
    FORCE_INLINE bool success() const { return error == ErrorCode::SUCCESS; }
    FORCE_INLINE bool failed() const { return error != ErrorCode::SUCCESS; }
};

} // namespace TradeKernel

#endif // TRADEKERNEL_TYPES_H
