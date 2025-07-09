#ifndef TRADEKERNEL_MEMORY_H
#define TRADEKERNEL_MEMORY_H

#include "types.h"

namespace TradeKernel {
namespace Memory {

// Memory pool configuration
struct PoolConfig {
    size_t block_size;
    size_t num_blocks;
    size_t alignment;
    bool lock_physical;
};

// Lock-free memory pool for deterministic allocation
class CACHE_ALIGNED LockFreePool {
private:
    struct Block {
        Block* next;
        u8 data[];
    } PACKED;
    
    Block* free_head;
    u8* memory_start;
    size_t block_size;
    size_t total_blocks;
    size_t alignment;
    
public:
    explicit LockFreePool(const PoolConfig& config);
    ~LockFreePool();
    
    FORCE_INLINE void* allocate() {
        Block* block = free_head;
        if (UNLIKELY(!block)) return nullptr;
        
        Block* next = block->next;
        if (__sync_bool_compare_and_swap(&free_head, block, next)) {
            return block->data;
        }
        return allocate(); // Retry on contention
    }
    
    FORCE_INLINE void deallocate(void* ptr) {
        if (UNLIKELY(!ptr)) return;
        
        Block* block = reinterpret_cast<Block*>(
            static_cast<u8*>(ptr) - sizeof(Block*));
        
        Block* head = free_head;
        do {
            block->next = head;
        } while (!__sync_bool_compare_and_swap(&free_head, head, block));
    }
    
    size_t available_blocks() const;
    bool is_pool_memory(void* ptr) const;
};

// NUMA-aware memory manager
class NumaMemoryManager {
private:
    static constexpr size_t MAX_NUMA_NODES = 8;
    
    struct NumaNode {
        u32 node_id;
        size_t total_memory;
        size_t available_memory;
        LockFreePool* pools[16]; // Different sized pools
    };
    
    NumaNode nodes[MAX_NUMA_NODES];
    u32 num_nodes;
    u32 current_cpu_node;
    
public:
    NumaMemoryManager();
    ~NumaMemoryManager();
    
    bool initialize();
    void* allocate(size_t size, u32 numa_node = UINT32_MAX);
    void deallocate(void* ptr);
    
    u32 get_current_numa_node() const { return current_cpu_node; }
    void set_cpu_affinity(u32 cpu);
    
private:
    u32 detect_numa_topology();
    void setup_pools_for_node(u32 node_id);
};

// DMA-capable memory regions
class DMAMemoryRegion {
private:
    void* virtual_addr;
    u64 physical_addr;
    size_t size;
    bool is_coherent;
    
public:
    DMAMemoryRegion(size_t size, bool coherent = true);
    ~DMAMemoryRegion();
    
    void* get_virtual_addr() const { return virtual_addr; }
    u64 get_physical_addr() const { return physical_addr; }
    size_t get_size() const { return size; }
    
    bool map_for_device();
    void sync_for_cpu();
    void sync_for_device();
};

// Memory statistics for monitoring
struct MemoryStats {
    size_t total_allocated;
    size_t peak_allocated;
    size_t num_allocations;
    size_t num_deallocations;
    nanoseconds_t avg_alloc_time;
    nanoseconds_t max_alloc_time;
};

// Global memory manager instance
extern NumaMemoryManager* g_memory_manager;

// Initialization and cleanup
bool initialize_memory_subsystem();
void shutdown_memory_subsystem();
MemoryStats get_memory_stats();

// Fast allocation functions
FORCE_INLINE void* fast_alloc(size_t size) {
    return g_memory_manager->allocate(size);
}

FORCE_INLINE void fast_free(void* ptr) {
    g_memory_manager->deallocate(ptr);
}

// Template allocator for containers
template<typename T>
class TradeAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    
    template<typename U>
    struct rebind {
        using other = TradeAllocator<U>;
    };
    
    TradeAllocator() = default;
    template<typename U>
    TradeAllocator(const TradeAllocator<U>&) {}
    
    pointer allocate(size_type n) {
        return static_cast<pointer>(fast_alloc(n * sizeof(T)));
    }
    
    void deallocate(pointer p, size_type) {
        fast_free(p);
    }
    
    template<typename U>
    bool operator==(const TradeAllocator<U>&) const { return true; }
    
    template<typename U>
    bool operator!=(const TradeAllocator<U>&) const { return false; }
};

} // namespace Memory
} // namespace TradeKernel

#endif // TRADEKERNEL_MEMORY_H
