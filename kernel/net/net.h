#ifndef NET_H
#define NET_H

#include "../types.h"

// Forward declarations
typedef struct tcp_connection tcp_connection_t;

// TCP states
typedef enum {
    TCP_CLOSED,
    TCP_LISTEN,
    TCP_SYN_SENT,
    TCP_SYN_RECEIVED,
    TCP_ESTABLISHED,
    TCP_FIN_WAIT_1,
    TCP_FIN_WAIT_2,
    TCP_CLOSE_WAIT,
    TCP_CLOSING,
    TCP_LAST_ACK,
    TCP_TIME_WAIT
} tcp_state_t;

// TCP flags
#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10
#define TCP_FLAG_URG 0x20

// Network constants
#define ETH_MTU             1500    // Maximum transmission unit
#define ETH_HEADER_SIZE     14      // Ethernet header size
#define IP_HEADER_SIZE      20      // IP header size
#define TCP_HEADER_SIZE     20      // TCP header size
#define UDP_HEADER_SIZE     8       // UDP header size

// Ethernet frame types
#define ETH_TYPE_IP         0x0800
#define ETH_TYPE_ARP        0x0806

// IP protocol types
#define IP_PROTO_ICMP       1
#define IP_PROTO_TCP        6
#define IP_PROTO_UDP        17

// Socket types
#define SOCK_STREAM         1       // TCP
#define SOCK_DGRAM          2       // UDP
#define SOCK_RAW            3       // Raw IP

// Socket address families
#define AF_INET             2       // IPv4

// Network errors
#define NET_SUCCESS         0
#define NET_ERROR           -1
#define NET_TIMEOUT         -2
#define NET_NO_MEMORY       -3
#define NET_INVALID         -4

// MAC address structure
typedef struct {
    uint8_t addr[6];
} __attribute__((packed)) mac_addr_t;

// IPv4 address structure
typedef struct {
    uint8_t addr[4];
} __attribute__((packed)) ipv4_addr_t;

// Ethernet header
typedef struct {
    mac_addr_t dst_mac;
    mac_addr_t src_mac;
    uint16_t ethertype;
} __attribute__((packed)) eth_header_t;

// IPv4 header
typedef struct {
    uint8_t version_ihl;
    uint8_t tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    ipv4_addr_t src_ip;
    ipv4_addr_t dst_ip;
} __attribute__((packed)) ipv4_header_t;

// TCP header
typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint16_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent_ptr;
} __attribute__((packed)) tcp_header_t;

// UDP header
typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed)) udp_header_t;

// Socket address structure
typedef struct {
    uint16_t family;
    uint16_t port;
    ipv4_addr_t ip;
    uint8_t padding[8];
} __attribute__((packed)) sockaddr_t;

// IPv4 socket address
typedef struct sockaddr_in {
    uint16_t sin_family;    // AF_INET
    uint16_t sin_port;      // Port number
    ipv4_addr_t sin_addr;   // IP address
    char sin_zero[8];       // Padding
} sockaddr_in_t;

// Network interface structure
typedef struct {
    mac_addr_t mac_addr;
    ipv4_addr_t ip_addr;
    ipv4_addr_t netmask;
    ipv4_addr_t gateway;
    char name[16];
    int mtu;
    int (*send_packet)(const void* data, uint32_t len);
    int (*recv_packet)(void* buffer, uint32_t len);
} net_interface_t;

// TCP connection structure (full definition)
typedef struct tcp_connection {
    tcp_state_t state;
    ipv4_addr_t local_ip;
    ipv4_addr_t remote_ip;
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint32_t window_size;
    struct tcp_connection* next;
} tcp_connection_t;

// Socket structure
typedef struct socket {
    int domain;
    int type;
    int protocol;
    union {
        tcp_connection_t* tcp_conn;
        // UDP and raw socket data would go here
    } data;
    struct socket* next;
} socket_t;

// Network functions
int net_init(void);
int net_interface_up(net_interface_t* iface);
int net_interface_down(net_interface_t* iface);

// Socket API
int socket_create(int domain, int type, int protocol);
int socket_bind(int sockfd, const sockaddr_t* addr);
int socket_listen(int sockfd, int backlog);
int socket_accept(int sockfd, sockaddr_t* addr);
int socket_connect(int sockfd, const sockaddr_t* addr);
int socket_send(int sockfd, const void* buf, uint32_t len);
int socket_recv(int sockfd, void* buf, uint32_t len);
int socket_close(int sockfd);

// Utility functions
void net_ip_to_string(const ipv4_addr_t* ip, char* str);
int net_string_to_ip(const char* str, ipv4_addr_t* ip);

// Protocol handlers
void net_handle_ethernet(const void* packet, uint32_t len);
void net_handle_ipv4(const void* packet, uint32_t len);
void net_handle_tcp(const void* packet, uint32_t len);
void net_handle_udp(const void* packet, uint32_t len);

// Network checksum calculation (Internet checksum)
static inline uint16_t net_checksum(const void* data, uint32_t len) {
    const uint16_t* ptr = (const uint16_t*)data;
    uint32_t sum = 0;
    
    // Sum all 16-bit words
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    
    // Add left-over byte, if any
    if (len > 0) {
        sum += *(const uint8_t*)ptr;
    }
    
    // Fold 32-bit sum to 16 bits
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~sum;
}

// Convert MAC address to string
static inline char* net_mac_to_string(const mac_addr_t* mac) {
    static char buffer[18]; // XX:XX:XX:XX:XX:XX\0
    // Simple sprintf implementation for MAC address
    char hex[] = "0123456789abcdef";
    int pos = 0;
    for (int i = 0; i < 6; i++) {
        if (i > 0) buffer[pos++] = ':';
        buffer[pos++] = hex[(mac->addr[i] >> 4) & 0xF];
        buffer[pos++] = hex[mac->addr[i] & 0xF];
    }
    buffer[pos] = '\0';
    return buffer;
}

#endif // NET_H