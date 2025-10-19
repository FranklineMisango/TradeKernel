#include "websocket.h"
#include "../mm/memory.h"
#include "../drivers/vga.h"

// Forward declarations for string functions
char* strdup(const char* s);
size_t strlen(const char* s);
int snprintf(char* str, size_t size, const char* format, ...);

// WebSocket GUID for handshake
#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

// Connect to WebSocket server
websocket_t* websocket_connect(const char* host, int port, const char* path) {
    if (!host || !path) {
        return NULL;
    }

    websocket_t* ws = (websocket_t*)kmalloc(sizeof(websocket_t));
    if (!ws) {
        return NULL;
    }

    ws->host = strdup(host);
    ws->path = strdup(path);
    if (!ws->host || !ws->path) {
        if (ws->host) kfree(ws->host);
        if (ws->path) kfree(ws->path);
        kfree(ws);
        return NULL;
    }
    ws->port = port;
    ws->connected = 0;

    // Create socket
    ws->sockfd = socket_create(AF_INET, SOCK_STREAM, 0);
    if (ws->sockfd < 0) {
        kfree(ws);
        return NULL;
    }

    // Connect to server
    sockaddr_in_t addr;
    addr.sin_family = AF_INET;
    addr.sin_port = port;
    // TODO: Resolve hostname to IP
    addr.sin_addr.addr[0] = 104;  // testnet.binance.vision placeholder
    addr.sin_addr.addr[1] = 18;
    addr.sin_addr.addr[2] = 42;
    addr.sin_addr.addr[3] = 102;

    if (socket_connect(ws->sockfd, (sockaddr_t*)&addr) < 0) {
        socket_close(ws->sockfd);
        kfree(ws->host);
        kfree(ws->path);
        kfree(ws);
        return NULL;
    }

    // Perform WebSocket handshake
    if (websocket_upgrade_connection(ws->sockfd, host, path) < 0) {
        socket_close(ws->sockfd);
        kfree(ws->host);
        kfree(ws->path);
        kfree(ws);
        return NULL;
    }

    ws->connected = 1;
    return ws;
}

// Send WebSocket text frame
int websocket_send_text(websocket_t* ws, const char* text) {
    if (!ws || !ws->connected) {
        return -1;
    }

    size_t text_len = strlen(text);
    size_t frame_size = sizeof(ws_frame_t) + text_len;

    ws_frame_t* frame = (ws_frame_t*)kmalloc(frame_size);
    if (!frame) {
        return -1;
    }

    // Fill frame header
    frame->fin = 1;
    frame->rsv1 = 0;
    frame->rsv2 = 0;
    frame->rsv3 = 0;
    frame->opcode = WS_OPCODE_TEXT;
    frame->mask = 0;  // Server doesn't mask

    if (text_len < 126) {
        frame->payload_len = text_len;
    } else if (text_len < 65536) {
        frame->payload_len = 126;
        frame->extended_len = text_len;
    } else {
        frame->payload_len = 127;
        frame->full_len = text_len;
    }

    // Copy payload
    memcpy(frame->payload, text, text_len);

    // Send frame
    int result = socket_send(ws->sockfd, frame, frame_size);
    kfree(frame);

    return result;
}

// Receive WebSocket frame
int websocket_recv_frame(websocket_t* ws, ws_frame_t** frame_out) {
    if (!ws || !ws->connected) {
        return -1;
    }

    // For now, this is a placeholder
    // In a real implementation, we'd read from the socket and parse frames
    *frame_out = NULL;
    return 0;
}

// Close WebSocket connection
void websocket_close(websocket_t* ws) {
    if (!ws) return;

    if (ws->connected) {
        // Send close frame
        ws_frame_t close_frame = {
            .fin = 1,
            .opcode = WS_OPCODE_CLOSE,
            .mask = 0,
            .payload_len = 0
        };
        socket_send(ws->sockfd, &close_frame, sizeof(ws_frame_t));
    }

    socket_close(ws->sockfd);
    kfree(ws->host);
    kfree(ws->path);
    kfree(ws);
}

// Perform WebSocket HTTP upgrade handshake
int websocket_upgrade_connection(int sockfd, const char* host, const char* path) {
    // Send HTTP upgrade request
    char request[512];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Upgrade: websocket\r\n"
             "Connection: Upgrade\r\n"
             "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
             "Sec-WebSocket-Version: 13\r\n"
             "\r\n",
             path, host);

    if (socket_send(sockfd, request, strlen(request)) < 0) {
        return -1;
    }

    // For now, assume handshake succeeds
    // In a real implementation, we'd read and parse the response
    return 0;
}

// Simple string duplicate (since we don't have standard library)
char* strdup(const char* s) {
    if (!s) {
        return NULL;
    }
    size_t len = strlen(s) + 1;
    char* dup = (char*)kmalloc(len);
    if (dup) {
        // Manual copy instead of memcpy to avoid potential issues
        for (size_t i = 0; i < len; i++) {
            dup[i] = s[i];
        }
    }
    return dup;
}

// Simple string length (extern from memory.c)
extern size_t strlen(const char* s);

// Simple string format (basic implementation)
int snprintf(char* str, size_t size, const char* format, ...) {
    if (!str || !format || size == 0) {
        return 0;
    }
    // Very basic implementation - just copy format for now
    size_t len = strlen(format);
    if (len >= size) len = size - 1;
    for (size_t i = 0; i < len; i++) {
        str[i] = format[i];
    }
    str[len] = '\0';
    return len;
}