#include "ip.h"
#include "eth.h"
#include "../mm/memory.h"
#include "../drivers/vga.h"

// Global IP state
static ipv4_addr_t our_ip = {{192, 168, 1, 100}};  // Default IP
static ipv4_addr_t netmask = {{255, 255, 255, 0}}; // Default netmask
static ipv4_addr_t gateway = {{192, 168, 1, 1}};   // Default gateway

// Initialize IP layer
int ipv4_init(void) {
    vga_write_string("Initializing IPv4 protocol...\n");
    return NET_SUCCESS;
}

// Send an IP packet
int ipv4_send_packet(const ipv4_addr_t* dst_ip, uint8_t protocol,
                     const void* data, uint32_t data_len) {
    // Calculate total packet size
    uint32_t total_len = sizeof(ipv4_header_t) + data_len;

    // Allocate packet buffer
    uint8_t* packet = (uint8_t*)kmalloc(total_len);
    if (!packet) {
        return NET_NO_MEMORY;
    }

    ipv4_header_t* header = (ipv4_header_t*)packet;

    // Fill IP header
    header->version_ihl = (4 << 4) | 5;  // IPv4, 5*4=20 byte header
    header->tos = 0;
    header->total_len = total_len;
    header->id = 0;  // TODO: proper ID assignment
    header->flags_frag = 0;
    header->ttl = 64;
    header->protocol = protocol;
    header->checksum = 0;
    header->src_ip = our_ip;
    header->dst_ip = *dst_ip;

    // Calculate checksum
    header->checksum = ipv4_checksum(header);

    // Copy data
    memcpy(packet + sizeof(ipv4_header_t), data, data_len);

    // Send via Ethernet
    int result = rtl8139_send_packet(packet, total_len);

    kfree(packet);
    return result;
}

// Handle incoming IP packet
int ipv4_handle_packet(const ipv4_packet_t* packet, uint32_t len) {
    const ipv4_header_t* header = &packet->header;

    // Basic validation
    if ((header->version_ihl >> 4) != 4) {
        return NET_INVALID; // Not IPv4
    }

    if (header->total_len > len) {
        return NET_INVALID; // Packet too short
    }

    // Verify checksum
    ipv4_header_t temp_header = *header;
    uint16_t checksum = temp_header.checksum;
    temp_header.checksum = 0;
    if (ipv4_checksum(&temp_header) != checksum) {
        return NET_INVALID; // Bad checksum
    }

    // Check if packet is for us
    if (!ipv4_is_our_address(&header->dst_ip)) {
        return NET_SUCCESS; // Not for us, ignore
    }

    // Route to appropriate protocol handler
    switch (header->protocol) {
        case IP_PROTO_TCP:
            // TODO: TCP handler
            break;
        case IP_PROTO_UDP:
            // TODO: UDP handler
            break;
        case IP_PROTO_ICMP:
            // TODO: ICMP handler
            break;
        default:
            // Unknown protocol, ignore
            break;
    }

    return NET_SUCCESS;
}

// Calculate IP header checksum
uint16_t ipv4_checksum(const ipv4_header_t* header) {
    return net_checksum(header, sizeof(ipv4_header_t));
}

// Check if IP address is ours
int ipv4_is_our_address(const ipv4_addr_t* ip) {
    return memcmp(ip, &our_ip, sizeof(ipv4_addr_t)) == 0;
}

// Get our IP address
const ipv4_addr_t* ipv4_get_our_address(void) {
    return &our_ip;
}