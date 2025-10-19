#include "tcp.h"
#include "ip.h"
#include "../mm/memory.h"
#include "../drivers/vga.h"

// Global TCP state
static tcp_connection_t* tcp_connections = NULL;
static uint32_t next_seq_num = 1000;  // Starting sequence number

// Forward declarations
tcp_connection_t* tcp_find_connection(const ipv4_addr_t* src_ip, const ipv4_addr_t* dst_ip,
                                      uint16_t src_port, uint16_t dst_port);
uint16_t tcp_checksum(const tcp_header_t* header, const void* data, uint32_t data_len,
                      const ipv4_addr_t* src_ip, const ipv4_addr_t* dst_ip);
const ipv4_addr_t* ipv4_get_our_address(void);

// Initialize TCP layer
int tcp_init(void) {
    vga_write_string("Initializing TCP protocol...\n");
    return NET_SUCCESS;
}

// Handle incoming TCP packet
int tcp_handle_packet(const tcp_packet_t* packet, uint32_t len,
                      const ipv4_addr_t* src_ip, const ipv4_addr_t* dst_ip) {
    const tcp_header_t* header = &packet->header;

    // Find matching connection
    tcp_connection_t* conn = tcp_find_connection(src_ip, dst_ip,
                                                 header->src_port, header->dst_port);

    if (!conn) {
        // No matching connection, check if it's a SYN for listening socket
        if (header->flags & TCP_FLAG_SYN) {
            // TODO: Handle new connection requests
            return NET_SUCCESS;
        }
        return NET_SUCCESS; // Ignore
    }

    // Update connection state based on flags
    if (header->flags & TCP_FLAG_SYN) {
        if (conn->state == TCP_LISTEN) {
            // Send SYN-ACK
            conn->state = TCP_SYN_RECEIVED;
            conn->ack_num = header->seq_num + 1;
            tcp_send_packet(conn, TCP_FLAG_SYN | TCP_FLAG_ACK, NULL, 0);
        }
    }

    if (header->flags & TCP_FLAG_ACK) {
        if (conn->state == TCP_SYN_SENT) {
            conn->state = TCP_ESTABLISHED;
        } else if (conn->state == TCP_SYN_RECEIVED) {
            conn->state = TCP_ESTABLISHED;
        }
    }

    if (header->flags & TCP_FLAG_FIN) {
        if (conn->state == TCP_ESTABLISHED) {
            conn->state = TCP_CLOSE_WAIT;
            conn->ack_num = header->seq_num + 1;
            tcp_send_packet(conn, TCP_FLAG_ACK, NULL, 0);
            // Send FIN
            tcp_send_packet(conn, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
            conn->state = TCP_LAST_ACK;
        }
    }

    return NET_SUCCESS;
}

// Send TCP packet
int tcp_send_packet(tcp_connection_t* conn, uint8_t flags,
                    const void* data, uint32_t data_len) {
    uint32_t total_len = sizeof(tcp_header_t) + data_len;

    // Allocate packet buffer
    uint8_t* packet = (uint8_t*)kmalloc(total_len);
    if (!packet) {
        return NET_NO_MEMORY;
    }

    tcp_header_t* header = (tcp_header_t*)packet;

    // Fill TCP header
    header->src_port = conn->local_port;
    header->dst_port = conn->remote_port;
    header->seq_num = conn->seq_num;
    header->ack_num = conn->ack_num;
    header->flags = flags;
    header->window = 65535;  // TODO: proper window management
    header->checksum = 0;
    header->urgent_ptr = 0;

    // Copy data
    if (data && data_len) {
        memcpy(packet + sizeof(tcp_header_t), data, data_len);
    }

    // Calculate checksum (includes pseudo-header)
    header->checksum = tcp_checksum(header, data, data_len, &conn->local_ip, &conn->remote_ip);

    // Send via IP
    int result = ipv4_send_packet(&conn->remote_ip, IP_PROTO_TCP, packet, total_len);

    // Update sequence number
    if (flags & TCP_FLAG_SYN || flags & TCP_FLAG_FIN || data_len > 0) {
        conn->seq_num += (flags & TCP_FLAG_SYN ? 1 : 0) + (flags & TCP_FLAG_FIN ? 1 : 0) + data_len;
    }

    kfree(packet);
    return result;
}

// Create new TCP connection
tcp_connection_t* tcp_create_connection(const ipv4_addr_t* remote_ip,
                                        uint16_t remote_port, uint16_t local_port) {
    tcp_connection_t* conn = (tcp_connection_t*)kmalloc(sizeof(tcp_connection_t));
    if (!conn) {
        return NULL;
    }

    conn->state = TCP_CLOSED;
    conn->local_ip = *ipv4_get_our_address();
    conn->remote_ip = *remote_ip;
    conn->local_port = local_port;
    conn->remote_port = remote_port;
    conn->seq_num = next_seq_num++;
    conn->ack_num = 0;
    conn->window_size = 65535;
    conn->next = tcp_connections;

    tcp_connections = conn;
    return conn;
}

// Close TCP connection
void tcp_close_connection(tcp_connection_t* conn) {
    if (!conn) return;

    // Remove from connection list
    if (tcp_connections == conn) {
        tcp_connections = conn->next;
    } else {
        tcp_connection_t* prev = tcp_connections;
        while (prev && prev->next != conn) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = conn->next;
        }
    }

    kfree(conn);
}

// Find connection by addresses and ports
tcp_connection_t* tcp_find_connection(const ipv4_addr_t* src_ip, const ipv4_addr_t* dst_ip,
                                      uint16_t src_port, uint16_t dst_port) {
    tcp_connection_t* conn = tcp_connections;
    while (conn) {
        if (memcmp(&conn->local_ip, dst_ip, sizeof(ipv4_addr_t)) == 0 &&
            memcmp(&conn->remote_ip, src_ip, sizeof(ipv4_addr_t)) == 0 &&
            conn->local_port == dst_port &&
            conn->remote_port == src_port) {
            return conn;
        }
        conn = conn->next;
    }
    return NULL;
}

// Calculate TCP checksum (includes pseudo-header)
uint16_t tcp_checksum(const tcp_header_t* header, const void* data, uint32_t data_len,
                      const ipv4_addr_t* src_ip, const ipv4_addr_t* dst_ip) {
    // TCP pseudo-header
    struct {
        ipv4_addr_t src_ip;
        ipv4_addr_t dst_ip;
        uint8_t zero;
        uint8_t protocol;
        uint16_t tcp_len;
    } __attribute__((packed)) pseudo_header = {
        .src_ip = *src_ip,
        .dst_ip = *dst_ip,
        .zero = 0,
        .protocol = IP_PROTO_TCP,
        .tcp_len = sizeof(tcp_header_t) + data_len
    };

    // Calculate total length for checksum
    uint32_t total_len = sizeof(pseudo_header) + sizeof(tcp_header_t) + data_len;
    uint8_t* buffer = (uint8_t*)kmalloc(total_len);
    if (!buffer) {
        return 0;
    }

    // Copy pseudo-header
    memcpy(buffer, &pseudo_header, sizeof(pseudo_header));
    // Copy TCP header
    memcpy(buffer + sizeof(pseudo_header), header, sizeof(tcp_header_t));
    // Copy data
    if (data && data_len) {
        memcpy(buffer + sizeof(pseudo_header) + sizeof(tcp_header_t), data, data_len);
    }

    uint16_t checksum = net_checksum(buffer, total_len);
    kfree(buffer);
    return checksum;
}