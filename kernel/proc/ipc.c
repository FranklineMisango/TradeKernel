#include "ipc.h"
#include "../mm/memory.h"
#include "../drivers/vga.h"
#include "process.h"
#include "../arch/interrupts.h"

// Remove static memcpy/memset implementations - use the ones from memory.h

// Global IPC structures
static message_queue_t message_queues[MAX_MESSAGE_QUEUES];
static semaphore_t semaphores[MAX_SEMAPHORES];
static uint32_t next_msgq_id = 1;
static uint32_t next_sem_id = 1;

extern process_t* current_process;
extern uint32_t get_ticks(void);

void ipc_init(void) {
    // Initialize message queues
    for (int i = 0; i < MAX_MESSAGE_QUEUES; i++) {
        message_queues[i].in_use = 0;
        message_queues[i].id = 0;
        message_queues[i].head = 0;
        message_queues[i].tail = 0;
        message_queues[i].count = 0;
    }
    
    // Initialize semaphores
    for (int i = 0; i < MAX_SEMAPHORES; i++) {
        semaphores[i].in_use = 0;
        semaphores[i].id = 0;
        semaphores[i].value = 0;
        semaphores[i].wait_count = 0;
    }
    
    vga_write_string("IPC subsystem initialized\n");
}

// Message queue implementation
uint32_t msgget(uint32_t key, uint32_t flags) {
    // Look for existing queue with same key
    for (int i = 0; i < MAX_MESSAGE_QUEUES; i++) {
        if (message_queues[i].in_use && message_queues[i].key == key) {
            return message_queues[i].id;
        }
    }
    
    // Create new queue if IPC_CREAT flag is set
    if (flags & 0x200) { // IPC_CREAT
        for (int i = 0; i < MAX_MESSAGE_QUEUES; i++) {
            if (!message_queues[i].in_use) {
                message_queues[i].in_use = 1;
                message_queues[i].id = next_msgq_id++;
                message_queues[i].key = key;
                message_queues[i].head = 0;
                message_queues[i].tail = 0;
                message_queues[i].count = 0;
                message_queues[i].max_size = MAX_QUEUE_SIZE;
                message_queues[i].permissions = flags & 0777;
                message_queues[i].creator_pid = current_process ? current_process->pid : 0;
                
                return message_queues[i].id;
            }
        }
    }
    
    return -1; // No queue found or created
}

int msgsnd(uint32_t msgid, const message_t* msg, uint32_t size, uint32_t flags) {
    if (!msg || size > MAX_MESSAGE_SIZE) {
        return -1;
    }
    
    // Find message queue
    message_queue_t* queue = NULL;
    for (int i = 0; i < MAX_MESSAGE_QUEUES; i++) {
        if (message_queues[i].in_use && message_queues[i].id == msgid) {
            queue = &message_queues[i];
            break;
        }
    }
    
    if (!queue) {
        return -1;
    }
    
    // Check if queue is full
    if (queue->count >= queue->max_size) {
        if (flags & 0x800) { // IPC_NOWAIT
            return -1;
        }
        // In full implementation, would block here
        return -1;
    }
    
    // Add message to queue
    message_t* slot = &queue->messages[queue->tail];
    memcpy(slot, msg, sizeof(message_t));
    slot->sender_pid = current_process ? current_process->pid : 0;
    slot->timestamp = get_ticks();
    slot->size = size;
    
    queue->tail = (queue->tail + 1) % queue->max_size;
    queue->count++;
    
    return 0;
}

int msgrcv(uint32_t msgid, message_t* msg, uint32_t size, uint32_t type, uint32_t flags) {
    if (!msg) {
        return -1;
    }
    
    // Find message queue
    message_queue_t* queue = NULL;
    for (int i = 0; i < MAX_MESSAGE_QUEUES; i++) {
        if (message_queues[i].in_use && message_queues[i].id == msgid) {
            queue = &message_queues[i];
            break;
        }
    }
    
    if (!queue) {
        return -1;
    }
    
    // Look for message of specified type (or any type if type == 0)
    for (uint32_t i = 0; i < queue->count; i++) {
        uint32_t index = (queue->head + i) % queue->max_size;
        message_t* candidate = &queue->messages[index];
        
        if (type == 0 || candidate->type == type) {
            // Found matching message
            uint32_t msg_size = candidate->size;
            if (msg_size > size) {
                return -1; // Message too large
            }
            
            // Copy message
            memcpy(msg, candidate, sizeof(message_t));
            
            // Remove from queue by shifting remaining messages
            for (uint32_t j = i; j < queue->count - 1; j++) {
                uint32_t src_idx = (queue->head + j + 1) % queue->max_size;
                uint32_t dst_idx = (queue->head + j) % queue->max_size;
                memcpy(&queue->messages[dst_idx], &queue->messages[src_idx], sizeof(message_t));
            }
            
            queue->count--;
            
            return msg_size;
        }
    }
    
    // No matching message found
    if (flags & 0x800) { // IPC_NOWAIT
        return -1;
    }
    
    // In full implementation, would block here
    return -1;
}

int msgctl(uint32_t msgid, uint32_t cmd, void* buf) {
    (void)buf;  // Suppress unused parameter warning - TODO: implement control operations
    
    // Find message queue
    message_queue_t* queue = NULL;
    for (int i = 0; i < MAX_MESSAGE_QUEUES; i++) {
        if (message_queues[i].in_use && message_queues[i].id == msgid) {
            queue = &message_queues[i];
            break;
        }
    }
    
    if (!queue) {
        return -1;
    }
    
    if (cmd == 0) { // IPC_RMID - remove queue
        queue->in_use = 0;
        return 0;
    }
    
    return -1;
}

// Semaphore implementation
uint32_t semget(uint32_t key, uint32_t nsems, uint32_t flags) {
    (void)nsems;  // Suppress unused parameter warning - TODO: implement multi-semaphore sets
    
    // Look for existing semaphore with same key
    for (int i = 0; i < MAX_SEMAPHORES; i++) {
        if (semaphores[i].in_use && semaphores[i].key == key) {
            return semaphores[i].id;
        }
    }
    
    // Create new semaphore if IPC_CREAT flag is set
    if (flags & 0x200) { // IPC_CREAT
        for (int i = 0; i < MAX_SEMAPHORES; i++) {
            if (!semaphores[i].in_use) {
                semaphores[i].in_use = 1;
                semaphores[i].id = next_sem_id++;
                semaphores[i].key = key;
                semaphores[i].value = 0;
                semaphores[i].max_value = 1;
                semaphores[i].wait_count = 0;
                semaphores[i].permissions = flags & 0777;
                semaphores[i].creator_pid = current_process ? current_process->pid : 0;
                
                return semaphores[i].id;
            }
        }
    }
    
    return -1;
}

int semop(uint32_t semid, sembuf_t* ops, uint32_t nops) {
    (void)semid; (void)ops; (void)nops;  // Suppress unused parameter warnings - TODO: implement semaphore operations
    // Simplified semaphore operations
    // In full implementation, would support multiple operations atomically
    return -1; // Not fully implemented
}

int semctl(uint32_t semid, uint32_t semnum, uint32_t cmd, void* arg) {
    (void)semnum;  // Suppress unused parameter warning - TODO: implement multi-semaphore operations
    
    // Find semaphore
    semaphore_t* sem = NULL;
    for (int i = 0; i < MAX_SEMAPHORES; i++) {
        if (semaphores[i].in_use && semaphores[i].id == semid) {
            sem = &semaphores[i];
            break;
        }
    }
    
    if (!sem) {
        return -1;
    }
    
    if (cmd == 0) { // IPC_RMID - remove semaphore
        sem->in_use = 0;
        return 0;
    } else if (cmd == 16) { // SETVAL - set value
        sem->value = *(int*)arg;
        return 0;
    } else if (cmd == 12) { // GETVAL - get value
        return sem->value;
    }
    
    return -1;
}

// Trading-specific IPC functions
int send_market_data(uint32_t queue_id, const market_data_t* data) {
    message_t msg;
    msg.type = MSG_MARKET_DATA;
    msg.priority = 1; // High priority for market data
    memcpy(msg.data, data, sizeof(market_data_t));
    
    return msgsnd(queue_id, &msg, sizeof(market_data_t), 0);
}

int receive_market_data(uint32_t queue_id, market_data_t* data) {
    message_t msg;
    int result = msgrcv(queue_id, &msg, sizeof(market_data_t), MSG_MARKET_DATA, 0);
    
    if (result > 0) {
        memcpy(data, msg.data, sizeof(market_data_t));
        return 0;
    }
    
    return -1;
}

int send_order(uint32_t queue_id, const order_t* order) {
    message_t msg;
    msg.type = MSG_ORDER_REQUEST;
    msg.priority = 0; // Highest priority for orders
    memcpy(msg.data, order, sizeof(order_t));
    
    return msgsnd(queue_id, &msg, sizeof(order_t), 0);
}

int receive_order(uint32_t queue_id, order_t* order) {
    message_t msg;
    int result = msgrcv(queue_id, &msg, sizeof(order_t), MSG_ORDER_REQUEST, 0);
    
    if (result > 0) {
        memcpy(order, msg.data, sizeof(order_t));
        return 0;
    }
    
    return -1;
}

int broadcast_trade_signal(uint32_t signal_type, const void* data, uint32_t size) {
    (void)signal_type;  // Suppress unused parameter warning - TODO: implement signal type handling
    
    // Broadcast to all interested processes
    // In full implementation, would maintain subscriber lists
    message_t msg;
    msg.type = MSG_TRADE_SIGNAL;
    msg.priority = 2;
    memcpy(msg.data, data, size);
    
    // For now, just demonstrate the concept
    vga_write_string("Broadcasting trade signal\n");
    return 0;
}

int send_priority_message(uint32_t queue_id, uint32_t type, const void* data, 
                         uint32_t size, uint32_t priority) {
    message_t msg;
    msg.type = type;
    msg.priority = priority;
    memcpy(msg.data, data, size);
    
    return msgsnd(queue_id, &msg, size, 0);
}

int receive_priority_message(uint32_t queue_id, uint32_t type, void* data, 
                            uint32_t max_size, uint32_t timeout) {
    (void)timeout;  // Suppress unused parameter warning - TODO: implement timeout functionality
    
    message_t msg;
    int result = msgrcv(queue_id, &msg, max_size, type, 0x800); // IPC_NOWAIT
    
    if (result > 0) {
        memcpy(data, msg.data, result);
        return result;
    }
    
    return -1;
}

// Lock-free ring buffer implementation for ultra-low latency
int ringbuf_init(lockfree_ringbuf_t* rb, uint32_t size, uint32_t element_size) {
    if (!rb || size == 0 || element_size == 0) {
        return -1;
    }
    
    // Ensure size is power of 2 for efficient modulo
    uint32_t power_of_two = 1;
    while (power_of_two < size) {
        power_of_two <<= 1;
    }
    
    rb->buffer = kmalloc(power_of_two * element_size);
    if (!rb->buffer) {
        return -1;
    }
    
    rb->size = power_of_two;
    rb->mask = power_of_two - 1;
    rb->element_size = element_size;
    rb->head = 0;
    rb->tail = 0;
    
    return 0;
}

int ringbuf_push(lockfree_ringbuf_t* rb, const void* data) {
    if (!rb || !data) {
        return -1;
    }
    
    uint32_t head = rb->head;
    uint32_t tail = rb->tail;
    uint32_t next_tail = (tail + 1) & rb->mask;
    
    // Check if buffer is full
    if (next_tail == head) {
        return -1; // Buffer full
    }
    
    // Copy data to buffer
    uint8_t* slot = rb->buffer + (tail * rb->element_size);
    memcpy(slot, data, rb->element_size);
    
    // Update tail (memory barrier would be needed in SMP)
    rb->tail = next_tail;
    
    return 0;
}

int ringbuf_pop(lockfree_ringbuf_t* rb, void* data) {
    if (!rb || !data) {
        return -1;
    }
    
    uint32_t head = rb->head;
    uint32_t tail = rb->tail;
    
    // Check if buffer is empty
    if (head == tail) {
        return -1; // Buffer empty
    }
    
    // Copy data from buffer
    uint8_t* slot = rb->buffer + (head * rb->element_size);
    memcpy(data, slot, rb->element_size);
    
    // Update head (memory barrier would be needed in SMP)
    rb->head = (head + 1) & rb->mask;
    
    return 0;
}

uint32_t ringbuf_count(const lockfree_ringbuf_t* rb) {
    if (!rb) {
        return 0;
    }
    
    return (rb->tail - rb->head) & rb->mask;
}

// Shared memory pools for trading data
shared_pool_t* create_shared_pool(uint32_t element_size, uint32_t max_elements) {
    shared_pool_t* pool = kmalloc(sizeof(shared_pool_t));
    if (!pool) {
        return NULL;
    }
    
    pool->element_size = element_size;
    pool->max_elements = max_elements;
    pool->used_elements = 0;
    
    // Allocate memory for elements
    pool->size = element_size * max_elements;
    pool->base_addr = kmalloc(pool->size);
    if (!pool->base_addr) {
        kfree(pool);
        return NULL;
    }
    
    // Allocate bitmap for tracking allocations
    uint32_t bitmap_size = (max_elements + 7) / 8; // Round up to bytes
    pool->allocation_bitmap = kmalloc(bitmap_size);
    if (!pool->allocation_bitmap) {
        kfree(pool->base_addr);
        kfree(pool);
        return NULL;
    }
    
    memset(pool->allocation_bitmap, 0, bitmap_size);
    
    return pool;
}

void* shared_pool_alloc(shared_pool_t* pool) {
    if (!pool || pool->used_elements >= pool->max_elements) {
        return NULL;
    }
    
    // Find free slot
    for (uint32_t i = 0; i < pool->max_elements; i++) {
        uint32_t byte_idx = i / 8;
        uint32_t bit_idx = i % 8;
        
        if (!(pool->allocation_bitmap[byte_idx] & (1 << bit_idx))) {
            // Mark as allocated
            pool->allocation_bitmap[byte_idx] |= (1 << bit_idx);
            pool->used_elements++;
            
            // Return pointer to element
            return (uint8_t*)pool->base_addr + (i * pool->element_size);
        }
    }
    
    return NULL;
}

void shared_pool_free(shared_pool_t* pool, void* ptr) {
    if (!pool || !ptr) {
        return;
    }
    
    // Calculate index
    uintptr_t offset = (uintptr_t)ptr - (uintptr_t)pool->base_addr;
    uint32_t index = offset / pool->element_size;
    
    if (index >= pool->max_elements) {
        return; // Invalid pointer
    }
    
    uint32_t byte_idx = index / 8;
    uint32_t bit_idx = index % 8;
    
    // Mark as free
    if (pool->allocation_bitmap[byte_idx] & (1 << bit_idx)) {
        pool->allocation_bitmap[byte_idx] &= ~(1 << bit_idx);
        pool->used_elements--;
    }
}

void destroy_shared_pool(shared_pool_t* pool) {
    if (!pool) {
        return;
    }
    
    if (pool->base_addr) {
        kfree(pool->base_addr);
    }
    if (pool->allocation_bitmap) {
        kfree(pool->allocation_bitmap);
    }
    kfree(pool);
}