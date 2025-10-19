#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include "socket.h"

// WebSocket frame opcodes
#define WS_OPCODE_CONTINUATION 0x0
#define WS_OPCODE_TEXT        0x1
#define WS_OPCODE_BINARY      0x2
#define WS_OPCODE_CLOSE       0x8
#define WS_OPCODE_PING        0x9
#define WS_OPCODE_PONG        0xA

// WebSocket frame structure
typedef struct {
    uint8_t fin : 1;
    uint8_t rsv1 : 1;
    uint8_t rsv2 : 1;
    uint8_t rsv3 : 1;
    uint8_t opcode : 4;
    uint8_t mask : 1;
    uint8_t payload_len : 7;
    uint16_t extended_len;  // if payload_len == 126
    uint64_t full_len;      // if payload_len == 127
    uint32_t masking_key;
    uint8_t payload[];
} __attribute__((packed)) ws_frame_t;

// WebSocket connection structure
typedef struct websocket {
    int sockfd;
    char* host;
    char* path;
    int port;
    int connected;
} websocket_t;

// Function declarations
websocket_t* websocket_connect(const char* host, int port, const char* path);
int websocket_send_text(websocket_t* ws, const char* text);
int websocket_recv_frame(websocket_t* ws, ws_frame_t** frame);
void websocket_close(websocket_t* ws);
int websocket_upgrade_connection(int sockfd, const char* host, const char* path);

#endif