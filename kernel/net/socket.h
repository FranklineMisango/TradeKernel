#ifndef SOCKET_H
#define SOCKET_H

#include "net.h"

// Socket types
#define SOCK_STREAM 1    // TCP
#define SOCK_DGRAM  2    // UDP
#define SOCK_RAW    3    // Raw IP

// Address families
#define AF_INET    2     // IPv4

// Function declarations
int socket_create(int domain, int type, int protocol);
int socket_bind(int sockfd, const sockaddr_t* addr);
int socket_listen(int sockfd, int backlog);
int socket_accept(int sockfd, sockaddr_t* addr);
int socket_connect(int sockfd, const sockaddr_t* addr);
int socket_send(int sockfd, const void* buf, uint32_t len);
int socket_recv(int sockfd, void* buf, uint32_t len);
int socket_close(int sockfd);

// Helper functions
socket_t* socket_get(int sockfd);
int socket_alloc_fd(void);
void socket_free_fd(int fd);

#endif