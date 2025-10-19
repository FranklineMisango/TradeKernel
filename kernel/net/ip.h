#ifndef IP_H
#define IP_H

#include "net.h"

// IP packet structure
typedef struct {
    ipv4_header_t header;
    uint8_t data[];
} __attribute__((packed)) ipv4_packet_t;

// IP functions
int ipv4_init(void);
int ipv4_send_packet(const ipv4_addr_t* dst_ip, uint8_t protocol,
                     const void* data, uint32_t data_len);
int ipv4_handle_packet(const ipv4_packet_t* packet, uint32_t len);

// Utility functions
uint16_t ipv4_checksum(const ipv4_header_t* header);
int ipv4_is_our_address(const ipv4_addr_t* ip);
void ipv4_set_address(const ipv4_addr_t* ip, const ipv4_addr_t* netmask, const ipv4_addr_t* gateway);

#endif // IP_H