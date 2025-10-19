#ifndef IPC_H
#define IPC_H

#include "../types.h"

// Message queue constants
#define MAX_MESSAGE_QUEUES 32
#define MAX_MESSAGE_SIZE 1024
#define MAX_QUEUE_SIZE 64

// Semaphore constants
#define MAX_SEMAPHORES 64

// Message types for trading algorithms
#define MSG_MARKET_DATA     1
#define MSG_ORDER_REQUEST   2
#define MSG_ORDER_RESPONSE  3
#define MSG_TRADE_SIGNAL    4
#define MSG_RISK_UPDATE     5
#define MSG_PORTFOLIO_DATA  6
#define MSG_SYSTEM_ALERT    7

// Message structure
typedef struct {
    uint32_t type;
    uint32_t sender_pid;
    uint32_t size;
    uint8_t data[MAX_MESSAGE_SIZE];
    uint32_t timestamp;
    uint32_t priority;
} message_t;

// Message queue structure
typedef struct {
    uint32_t id;
    uint32_t key;
    message_t messages[MAX_QUEUE_SIZE];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    uint32_t max_size;
    uint32_t permissions;
    uint32_t creator_pid;
    uint8_t in_use;
} message_queue_t;

// Semaphore structure
typedef struct {
    uint32_t id;
    uint32_t key;
    int32_t value;
    uint32_t max_value;
    uint32_t wait_count;
    uint32_t permissions;
    uint32_t creator_pid;
    uint8_t in_use;
} semaphore_t;

// Trading-specific shared data structures
typedef struct {
    double price;
    uint64_t volume;
    uint32_t timestamp;
    uint16_t symbol_id;
    uint8_t side;       // 0=bid, 1=ask
    uint8_t flags;
} market_data_t;

typedef struct {
    uint32_t order_id;
    uint16_t symbol_id;
    uint8_t side;       // 0=buy, 1=sell
    uint8_t type;       // 0=market, 1=limit, 2=stop
    double price;
    uint64_t quantity;
    uint32_t timestamp;
    uint32_t client_id;
    uint8_t status;     // 0=pending, 1=filled, 2=cancelled
} order_t;

typedef struct {
    uint32_t position_id;
    uint16_t symbol_id;
    int64_t quantity;   // Positive=long, negative=short
    double avg_price;
    double unrealized_pnl;
    double realized_pnl;
    uint32_t timestamp;
} position_t;

// IPC management functions
void ipc_init(void);

// Message queue functions
uint32_t msgget(uint32_t key, uint32_t flags);
int msgsnd(uint32_t msgid, const message_t* msg, uint32_t size, uint32_t flags);
int msgrcv(uint32_t msgid, message_t* msg, uint32_t size, uint32_t type, uint32_t flags);
int msgctl(uint32_t msgid, uint32_t cmd, void* buf);

// Semaphore operation structure
typedef struct sembuf {
    uint16_t sem_num;    // semaphore number
    int16_t sem_op;      // semaphore operation
    int16_t sem_flg;     // operation flags
} sembuf_t;

// Semaphore functions
uint32_t semget(uint32_t key, uint32_t nsems, uint32_t flags);
int semop(uint32_t semid, sembuf_t* ops, uint32_t nops);
int semctl(uint32_t semid, uint32_t semnum, uint32_t cmd, void* arg);

// Trading-specific IPC functions
int send_market_data(uint32_t queue_id, const market_data_t* data);
int receive_market_data(uint32_t queue_id, market_data_t* data);
int send_order(uint32_t queue_id, const order_t* order);
int receive_order(uint32_t queue_id, order_t* order);
int broadcast_trade_signal(uint32_t signal_type, const void* data, uint32_t size);

// Real-time messaging for high-frequency trading
int send_priority_message(uint32_t queue_id, uint32_t type, const void* data, 
                         uint32_t size, uint32_t priority);
int receive_priority_message(uint32_t queue_id, uint32_t type, void* data, 
                            uint32_t max_size, uint32_t timeout);

// Lock-free ring buffer for ultra-low latency
typedef struct {
    volatile uint32_t head;
    volatile uint32_t tail;
    uint32_t size;
    uint32_t mask;
    uint8_t* buffer;
    uint32_t element_size;
} lockfree_ringbuf_t;

int ringbuf_init(lockfree_ringbuf_t* rb, uint32_t size, uint32_t element_size);
int ringbuf_push(lockfree_ringbuf_t* rb, const void* data);
int ringbuf_pop(lockfree_ringbuf_t* rb, void* data);
uint32_t ringbuf_count(const lockfree_ringbuf_t* rb);

// Shared memory pools for trading data
typedef struct {
    void* base_addr;
    uint32_t size;
    uint32_t element_size;
    uint32_t max_elements;
    uint32_t used_elements;
    uint8_t* allocation_bitmap;
} shared_pool_t;

shared_pool_t* create_shared_pool(uint32_t element_size, uint32_t max_elements);
void* shared_pool_alloc(shared_pool_t* pool);
void shared_pool_free(shared_pool_t* pool, void* ptr);
void destroy_shared_pool(shared_pool_t* pool);

#endif