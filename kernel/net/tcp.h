#ifndef TCP_H
#define TCP_H

#include "net.h"

// TCP flags and states are now defined in net.h

// TCP connection structure is now defined in net.h

// TCP packet structure
typedef struct {
    tcp_header_t header;
    uint8_t data[];
} __attribute__((packed)) tcp_packet_t;

// Function declarations
int tcp_init(void);
int tcp_handle_packet(const tcp_packet_t* packet, uint32_t len,
                      const ipv4_addr_t* src_ip, const ipv4_addr_t* dst_ip);
int tcp_send_packet(tcp_connection_t* conn, uint8_t flags,
                    const void* data, uint32_t data_len);
tcp_connection_t* tcp_create_connection(const ipv4_addr_t* remote_ip,
                                        uint16_t remote_port, uint16_t local_port);
void tcp_close_connection(tcp_connection_t* conn);
tcp_connection_t* tcp_find_connection(const ipv4_addr_t* src_ip, const ipv4_addr_t* dst_ip,
                                      uint16_t src_port, uint16_t dst_port);
uint16_t tcp_checksum(const tcp_header_t* header, const void* data, uint32_t data_len,
                      const ipv4_addr_t* src_ip, const ipv4_addr_t* dst_ip);

#endif