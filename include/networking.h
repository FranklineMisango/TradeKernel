#ifndef TRADEKERNEL_NETWORKING_H
#define TRADEKERNEL_NETWORKING_H

#include "types.h"
#include "memory.h"

namespace TradeKernel {
namespace Networking {

// Ethernet frame structure
struct PACKED EthernetHeader {
    u8 dst_mac[6];
    u8 src_mac[6];
    u16 ethertype;
};

// IP header structure
struct PACKED IPv4Header {
    u8 version_ihl;
    u8 dscp_ecn;
    u16 total_length;
    u16 identification;
    u16 flags_fragment;
    u8 ttl;
    u8 protocol;
    u16 checksum;
    u32 src_ip;
    u32 dst_ip;
};

// UDP header structure
struct PACKED UDPHeader {
    u16 src_port;
    u16 dst_port;
    u16 length;
    u16 checksum;
};

// Network packet descriptor
struct CACHE_ALIGNED NetworkPacket {
    u8* data;
    size_t length;
    u64 timestamp;
    PacketType type;
    u32 hash;
    u16 vlan_id;
    u8 priority;
    bool is_multicast;
    
    // Zero-copy operations
    EthernetHeader* get_eth_header() const {
        return reinterpret_cast<EthernetHeader*>(data);
    }
    
    IPv4Header* get_ip_header() const {
        return reinterpret_cast<IPv4Header*>(data + sizeof(EthernetHeader));
    }
    
    UDPHeader* get_udp_header() const {
        return reinterpret_cast<UDPHeader*>(data + sizeof(EthernetHeader) + sizeof(IPv4Header));
    }
    
    u8* get_payload() const {
        return data + sizeof(EthernetHeader) + sizeof(IPv4Header) + sizeof(UDPHeader);
    }
    
    size_t get_payload_size() const {
        return length - sizeof(EthernetHeader) - sizeof(IPv4Header) - sizeof(UDPHeader);
    }
};

// Ring buffer for zero-copy packet processing
template<size_t SIZE>
class CACHE_ALIGNED PacketRingBuffer {
private:
    static_assert((SIZE & (SIZE - 1)) == 0, "Ring buffer size must be power of 2");
    
    NetworkPacket packets[SIZE];
    volatile u32 producer_head;
    volatile u32 producer_tail;
    volatile u32 consumer_head;
    volatile u32 consumer_tail;
    
public:
    PacketRingBuffer() : producer_head(0), producer_tail(0), consumer_head(0), consumer_tail(0) {}
    
    FORCE_INLINE bool enqueue(const NetworkPacket& packet) {
        u32 next_tail = (producer_tail + 1) & (SIZE - 1);
        if (next_tail == consumer_head) {
            return false; // Ring buffer full
        }
        
        packets[producer_tail] = packet;
        WRITE_BARRIER();
        producer_tail = next_tail;
        
        return true;
    }
    
    FORCE_INLINE bool dequeue(NetworkPacket& packet) {
        if (consumer_head == producer_tail) {
            return false; // Ring buffer empty
        }
        
        packet = packets[consumer_head];
        READ_BARRIER();
        consumer_head = (consumer_head + 1) & (SIZE - 1);
        
        return true;
    }
    
    FORCE_INLINE bool empty() const {
        return consumer_head == producer_tail;
    }
    
    FORCE_INLINE bool full() const {
        u32 next_tail = (producer_tail + 1) & (SIZE - 1);
        return next_tail == consumer_head;
    }
    
    FORCE_INLINE size_t size() const {
        return (producer_tail - consumer_head) & (SIZE - 1);
    }
};

// Network interface abstraction
class NetworkInterface {
private:
    u32 interface_id;
    u8 mac_address[6];
    u32 ip_address;
    u16 mtu;
    bool promiscuous_mode;
    
    // Packet buffers
    PacketRingBuffer<4096> rx_ring;
    PacketRingBuffer<4096> tx_ring;
    
    // Statistics
    u64 packets_received;
    u64 packets_transmitted;
    u64 bytes_received;
    u64 bytes_transmitted;
    u64 rx_errors;
    u64 tx_errors;
    
public:
    NetworkInterface(u32 id, const u8* mac, u32 ip);
    ~NetworkInterface();
    
    // Packet operations
    bool receive_packet(NetworkPacket& packet);
    bool transmit_packet(const NetworkPacket& packet);
    
    // Configuration
    void set_promiscuous(bool enabled) { promiscuous_mode = enabled; }
    void set_mtu(u16 new_mtu) { mtu = new_mtu; }
    
    // Statistics
    struct NetStats {
        u64 rx_packets;
        u64 tx_packets;
        u64 rx_bytes;
        u64 tx_bytes;
        u64 rx_errors;
        u64 tx_errors;
        nanoseconds_t avg_rx_latency;
        nanoseconds_t avg_tx_latency;
    };
    
    NetStats get_stats() const;
    void reset_stats();
    
private:
    void process_received_packet(NetworkPacket& packet);
    bool validate_packet(const NetworkPacket& packet);
};

// Market data feed handler
class MarketDataHandler {
private:
    NetworkInterface* interface;
    PacketRingBuffer<8192> market_data_queue;
    
    // Market data statistics
    u64 messages_processed;
    nanoseconds_t avg_processing_time;
    nanoseconds_t max_processing_time;
    
public:
    explicit MarketDataHandler(NetworkInterface* nic);
    ~MarketDataHandler();
    
    // Process incoming market data
    void process_market_data();
    
    // Callbacks for different message types
    using MessageCallback = void(*)(const u8* data, size_t length, u64 timestamp);
    
    void set_quote_callback(MessageCallback callback);
    void set_trade_callback(MessageCallback callback);
    void set_book_callback(MessageCallback callback);
    
private:
    MessageCallback quote_callback;
    MessageCallback trade_callback;
    MessageCallback book_callback;
    
    void decode_message(const NetworkPacket& packet);
    PacketType classify_packet(const NetworkPacket& packet);
};

// Order execution network stack
class OrderExecutionStack {
private:
    NetworkInterface* interface;
    PacketRingBuffer<4096> order_queue;
    PacketRingBuffer<4096> execution_queue;
    
    // Sequence numbers for reliability
    u32 next_outbound_seq;
    u32 expected_inbound_seq;
    
public:
    explicit OrderExecutionStack(NetworkInterface* nic);
    ~OrderExecutionStack();
    
    // Order operations
    bool send_order(const u8* order_data, size_t length);
    bool receive_execution(u8* exec_data, size_t& length);
    
    // Session management
    bool establish_session();
    bool heartbeat();
    void handle_logout();
    
private:
    bool validate_sequence(u32 seq_num);
    void request_retransmission(u32 from_seq, u32 to_seq);
};

// Multicast receiver for market data
class MulticastReceiver {
private:
    static constexpr size_t MAX_GROUPS = 64;
    
    struct MulticastGroup {
        u32 group_ip;
        u16 port;
        bool active;
        u64 packets_received;
    };
    
    NetworkInterface* interface;
    MulticastGroup groups[MAX_GROUPS];
    size_t num_groups;
    
public:
    explicit MulticastReceiver(NetworkInterface* nic);
    ~MulticastReceiver();
    
    bool join_group(u32 group_ip, u16 port);
    bool leave_group(u32 group_ip, u16 port);
    void process_multicast_packets();
    
private:
    bool is_multicast_packet(const NetworkPacket& packet);
    int find_group(u32 group_ip, u16 port);
};

// Network statistics collector
class NetworkStatsCollector {
private:
    struct LatencyHistogram {
        u32 buckets[32]; // Nanosecond buckets: <100ns, <200ns, <500ns, etc.
        u64 total_samples;
    };
    
    LatencyHistogram rx_latency;
    LatencyHistogram tx_latency;
    LatencyHistogram processing_latency;
    
public:
    void record_rx_latency(nanoseconds_t latency);
    void record_tx_latency(nanoseconds_t latency);
    void record_processing_latency(nanoseconds_t latency);
    
    void print_histogram(const LatencyHistogram& hist, const char* name);
    void reset_stats();
    
    // Performance monitoring
    struct PerfMetrics {
        nanoseconds_t p50_rx_latency;
        nanoseconds_t p95_rx_latency;
        nanoseconds_t p99_rx_latency;
        nanoseconds_t max_rx_latency;
        u64 total_packets;
        double packet_loss_rate;
    };
    
    PerfMetrics get_performance_metrics() const;
};

// Global networking subsystem
class NetworkingSubsystem {
private:
    static constexpr size_t MAX_INTERFACES = 8;
    
    NetworkInterface* interfaces[MAX_INTERFACES];
    size_t num_interfaces;
    NetworkStatsCollector stats_collector;
    
    bool initialized;
    
public:
    NetworkingSubsystem();
    ~NetworkingSubsystem();
    
    bool initialize();
    void shutdown();
    
    // Interface management
    u32 add_interface(const u8* mac_address, u32 ip_address);
    NetworkInterface* get_interface(u32 id);
    
    // Packet processing
    void process_all_interfaces();
    
    // Statistics
    NetworkStatsCollector& get_stats() { return stats_collector; }
    
private:
    bool detect_network_hardware();
    void setup_interrupt_handlers();
};

// Global networking instance
extern NetworkingSubsystem* g_networking;

// Initialization functions
bool initialize_networking();
void shutdown_networking();

// Utility functions
FORCE_INLINE u32 ip_address(u8 a, u8 b, u8 c, u8 d) {
    return (static_cast<u32>(a) << 24) | 
           (static_cast<u32>(b) << 16) | 
           (static_cast<u32>(c) << 8) | 
           static_cast<u32>(d);
}

FORCE_INLINE u16 checksum(const void* data, size_t length) {
    const u16* ptr = static_cast<const u16*>(data);
    u32 sum = 0;
    
    while (length > 1) {
        sum += *ptr++;
        length -= 2;
    }
    
    if (length > 0) {
        sum += *reinterpret_cast<const u8*>(ptr);
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~static_cast<u16>(sum);
}

} // namespace Networking
} // namespace TradeKernel

#endif // TRADEKERNEL_NETWORKING_H
