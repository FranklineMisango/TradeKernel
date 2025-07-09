#include "../include/memory.h"
#include "../include/types.h"

namespace TradeKernel {
namespace Memory {

// Global memory manager instance
NumaMemoryManager* g_memory_manager = nullptr;

// Memory statistics
static MemoryStats g_memory_stats = {};

// Lock-free memory pool implementation
LockFreePool::LockFreePool(const PoolConfig& config) 
    : free_head(nullptr)
    , memory_start(nullptr)
    , block_size(config.block_size)
    , total_blocks(config.num_blocks)
    , alignment(config.alignment) {
    
    // Calculate total memory needed
    size_t header_size = sizeof(Block);
    size_t aligned_block_size = (block_size + alignment - 1) & ~(alignment - 1);
    size_t total_size = total_blocks * (header_size + aligned_block_size);
    
    // Allocate memory (would use mmap in real implementation)
    memory_start = static_cast<u8*>(aligned_alloc(4096, total_size));
    if (!memory_start) {
        return;
    }
    
    // Lock physical memory if requested
    if (config.lock_physical) {
        // mlock(memory_start, total_size); // Would use mlock in real implementation
    }
    
    // Initialize free list
    u8* current = memory_start;
    Block* prev_block = nullptr;
    
    for (size_t i = 0; i < total_blocks; i++) {
        Block* block = reinterpret_cast<Block*>(current);
        block->next = prev_block;
        prev_block = block;
        current += header_size + aligned_block_size;
    }
    
    free_head = prev_block;
}

LockFreePool::~LockFreePool() {
    if (memory_start) {
        free(memory_start);
    }
}

size_t LockFreePool::available_blocks() const {
    size_t count = 0;
    Block* current = free_head;
    while (current) {
        count++;
        current = current->next;
    }
    return count;
}

bool LockFreePool::is_pool_memory(void* ptr) const {
    if (!ptr || !memory_start) return false;
    
    u8* addr = static_cast<u8*>(ptr);
    size_t header_size = sizeof(Block);
    size_t aligned_block_size = (block_size + alignment - 1) & ~(alignment - 1);
    size_t total_size = total_blocks * (header_size + aligned_block_size);
    
    return addr >= memory_start && addr < (memory_start + total_size);
}

// NUMA Memory Manager implementation
NumaMemoryManager::NumaMemoryManager() 
    : num_nodes(0)
    , current_cpu_node(0) {
    
    for (u32 i = 0; i < MAX_NUMA_NODES; i++) {
        nodes[i].node_id = i;
        nodes[i].total_memory = 0;
        nodes[i].available_memory = 0;
        for (int j = 0; j < 16; j++) {
            nodes[i].pools[j] = nullptr;
        }
    }
}

NumaMemoryManager::~NumaMemoryManager() {
    for (u32 i = 0; i < num_nodes; i++) {
        for (int j = 0; j < 16; j++) {
            delete nodes[i].pools[j];
        }
    }
}

bool NumaMemoryManager::initialize() {
    // Detect NUMA topology
    num_nodes = detect_numa_topology();
    if (num_nodes == 0) {
        num_nodes = 1; // Fallback to single node
    }
    
    // Set up memory pools for each node
    for (u32 i = 0; i < num_nodes; i++) {
        setup_pools_for_node(i);
    }
    
    // Detect current CPU's NUMA node
    current_cpu_node = 0; // Simplified for now
    
    return true;
}

void* NumaMemoryManager::allocate(size_t size, u32 numa_node) {
    if (numa_node == UINT32_MAX) {
        numa_node = current_cpu_node;
    }
    
    if (numa_node >= num_nodes) {
        numa_node = 0;
    }
    
    // Find appropriate pool based on size
    int pool_index = 0;
    size_t pool_size = 64; // Start with 64-byte pool
    
    while (pool_index < 16 && pool_size < size) {
        pool_index++;
        pool_size *= 2;
    }
    
    if (pool_index >= 16) {
        return nullptr; // Size too large
    }
    
    LockFreePool* pool = nodes[numa_node].pools[pool_index];
    if (!pool) {
        return nullptr;
    }
    
    cycles_t start = rdtsc();
    void* ptr = pool->allocate();
    cycles_t end = rdtsc();
    
    if (ptr) {
        g_memory_stats.num_allocations++;
        g_memory_stats.total_allocated += pool_size;
        
        nanoseconds_t alloc_time = end - start; // Simplified conversion
        g_memory_stats.avg_alloc_time = 
            (g_memory_stats.avg_alloc_time + alloc_time) / 2;
        
        if (alloc_time > g_memory_stats.max_alloc_time) {
            g_memory_stats.max_alloc_time = alloc_time;
        }
    }
    
    return ptr;
}

void NumaMemoryManager::deallocate(void* ptr) {
    if (!ptr) return;
    
    // Find which pool this memory belongs to
    for (u32 node = 0; node < num_nodes; node++) {
        for (int pool_idx = 0; pool_idx < 16; pool_idx++) {
            LockFreePool* pool = nodes[node].pools[pool_idx];
            if (pool && pool->is_pool_memory(ptr)) {
                pool->deallocate(ptr);
                g_memory_stats.num_deallocations++;
                return;
            }
        }
    }
}

u32 NumaMemoryManager::detect_numa_topology() {
    // Simplified NUMA detection
    // In a real implementation, this would query ACPI SRAT tables
    return 1; // Single NUMA node for now
}

void NumaMemoryManager::setup_pools_for_node(u32 node_id) {
    // Create pools of different sizes: 64B, 128B, 256B, etc.
    size_t base_size = 64;
    
    for (int i = 0; i < 16; i++) {
        PoolConfig config;
        config.block_size = base_size << i;
        config.num_blocks = 1024 >> (i / 4); // Fewer blocks for larger sizes
        config.alignment = (config.block_size >= 64) ? 64 : config.block_size;
        config.lock_physical = true;
        
        nodes[node_id].pools[i] = new LockFreePool(config);
    }
    
    nodes[node_id].total_memory = 256 * 1024 * 1024; // 256MB per node
    nodes[node_id].available_memory = nodes[node_id].total_memory;
}

void NumaMemoryManager::set_cpu_affinity(u32 cpu) {
    // In a real implementation, this would set CPU affinity
    // and update current_cpu_node based on CPU topology
    current_cpu_node = cpu % num_nodes;
}

// DMA Memory Region implementation
DMAMemoryRegion::DMAMemoryRegion(size_t size, bool coherent)
    : virtual_addr(nullptr)
    , physical_addr(0)
    , size(size)
    , is_coherent(coherent) {
    
    // Allocate DMA-capable memory
    // In a real implementation, this would use DMA allocation APIs
    virtual_addr = aligned_alloc(4096, size);
    physical_addr = reinterpret_cast<u64>(virtual_addr); // Simplified
}

DMAMemoryRegion::~DMAMemoryRegion() {
    if (virtual_addr) {
        free(virtual_addr);
    }
}

bool DMAMemoryRegion::map_for_device() {
    // Map memory for device access
    return true; // Simplified
}

void DMAMemoryRegion::sync_for_cpu() {
    if (!is_coherent) {
        // Invalidate cache lines
        MEMORY_BARRIER();
    }
}

void DMAMemoryRegion::sync_for_device() {
    if (!is_coherent) {
        // Flush cache lines
        MEMORY_BARRIER();
    }
}

// Global functions
bool initialize_memory_subsystem() {
    if (g_memory_manager) {
        return false; // Already initialized
    }
    
    g_memory_manager = new NumaMemoryManager();
    if (!g_memory_manager) {
        return false;
    }
    
    return g_memory_manager->initialize();
}

void shutdown_memory_subsystem() {
    delete g_memory_manager;
    g_memory_manager = nullptr;
}

MemoryStats get_memory_stats() {
    return g_memory_stats;
}

} // namespace Memory
} // namespace TradeKernel
