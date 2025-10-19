#include "socket.h"
#include "../mm/memory.h"
#include "../drivers/vga.h"

// Global socket state
static socket_t* sockets = NULL;
static int next_fd = 3;  // Start after stdin/stdout/stderr

// Create a new socket
int socket_create(int domain, int type, int protocol) {
    if (domain != AF_INET) {
        return -1;  // Only IPv4 supported
    }

    socket_t* sock = (socket_t*)kmalloc(sizeof(socket_t));
    if (!sock) {
        return -1;
    }

    sock->domain = domain;
    sock->type = type;
    sock->protocol = protocol;
    sock->next = sockets;
    sockets = sock;

    int fd = socket_alloc_fd();
    if (fd < 0) {
        kfree(sock);
        return -1;
    }

    // Store socket pointer in some way - for now just return fd
    // In a real implementation, we'd have a file descriptor table
    return fd;
}

// Bind socket to address
int socket_bind(int sockfd, const sockaddr_t* addr) {
    socket_t* sock = socket_get(sockfd);
    if (!sock) {
        return -1;
    }

    const sockaddr_in_t* addr_in = (const sockaddr_in_t*)addr;

    if (sock->type == SOCK_STREAM) {
        // Create TCP connection in LISTEN state
        sock->data.tcp_conn = tcp_create_connection(&addr_in->sin_addr,
                                                    addr_in->sin_port, addr_in->sin_port);
        if (!sock->data.tcp_conn) {
            return -1;
        }
        sock->data.tcp_conn->state = TCP_LISTEN;
    }

    return 0;
}

// Listen for connections
int socket_listen(int sockfd, int backlog) {
    socket_t* sock = socket_get(sockfd);
    if (!sock || sock->type != SOCK_STREAM) {
        return -1;
    }

    // TCP is already in LISTEN state from bind
    return 0;
}

// Accept incoming connection
int socket_accept(int sockfd, sockaddr_t* addr) {
    socket_t* sock = socket_get(sockfd);
    if (!sock || sock->type != SOCK_STREAM) {
        return -1;
    }

    // For now, just return the same socket
    // In a real implementation, we'd create a new socket for the accepted connection
    if (addr) {
        // Fill in peer address
        sockaddr_in_t* addr_in = (sockaddr_in_t*)addr;
        addr_in->sin_family = AF_INET;
        addr_in->sin_port = sock->data.tcp_conn->remote_port;
        addr_in->sin_addr = sock->data.tcp_conn->remote_ip;
    }

    return sockfd;
}

// Connect to remote host
int socket_connect(int sockfd, const sockaddr_t* addr) {
    socket_t* sock = socket_get(sockfd);
    if (!sock) {
        return -1;
    }

    const sockaddr_in_t* addr_in = (const sockaddr_in_t*)addr;

    if (sock->type == SOCK_STREAM) {
        // Create TCP connection
        sock->data.tcp_conn = tcp_create_connection(&addr_in->sin_addr,
                                                    addr_in->sin_port, 0); // Auto-assign local port
        if (!sock->data.tcp_conn) {
            return -1;
        }

        // Send SYN packet
        sock->data.tcp_conn->state = TCP_SYN_SENT;
        tcp_send_packet(sock->data.tcp_conn, TCP_FLAG_SYN, NULL, 0);
    }

    return 0;
}

// Send data
int socket_send(int sockfd, const void* buf, uint32_t len) {
    socket_t* sock = socket_get(sockfd);
    if (!sock) {
        return -1;
    }

    if (sock->type == SOCK_STREAM && sock->data.tcp_conn) {
        if (sock->data.tcp_conn->state != TCP_ESTABLISHED) {
            return -1;  // Connection not established
        }

        return tcp_send_packet(sock->data.tcp_conn, TCP_FLAG_ACK, buf, len);
    }

    return -1;
}

// Receive data
int socket_recv(int sockfd, void* buf, uint32_t len) {
    // For now, this is a placeholder
    // In a real implementation, we'd have a receive buffer
    return 0;
}

// Close socket
int socket_close(int sockfd) {
    socket_t* sock = socket_get(sockfd);
    if (!sock) {
        return -1;
    }

    if (sock->type == SOCK_STREAM && sock->data.tcp_conn) {
        tcp_close_connection(sock->data.tcp_conn);
    }

    // Remove from socket list
    if (sockets == sock) {
        sockets = sock->next;
    } else {
        socket_t* prev = sockets;
        while (prev && prev->next != sock) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = sock->next;
        }
    }

    socket_free_fd(sockfd);
    kfree(sock);
    return 0;
}

// Get socket by file descriptor
socket_t* socket_get(int sockfd) {
    // For now, just return the first socket
    // In a real implementation, we'd have a proper FD table
    return sockets;
}

// Allocate file descriptor
int socket_alloc_fd(void) {
    return next_fd++;
}

// Free file descriptor
void socket_free_fd(int fd) {
    // Placeholder - in real implementation, mark FD as free
}