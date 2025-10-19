#ifndef ETH_H
#define ETH_H

#include "../types.h"
#include "net.h"

// I/O port functions (declared here since we don't have standard headers)
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline void outl(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// RTL8139 registers
#define RTL8139_BASE        0xC000  // Base I/O port (configurable)
#define RTL8139_IDR0        0x00    // MAC address registers
#define RTL8139_TSD0        0x10    // Transmit status descriptor
#define RTL8139_TSAD0       0x20    // Transmit start address descriptor
#define RTL8139_RBSTART     0x30    // Receive buffer start address
#define RTL8139_CR          0x37    // Command register
#define RTL8139_CAPR        0x38    // Current address of packet read
#define RTL8139_CBR         0x3A    // Current buffer address
#define RTL8139_IMR         0x3C    // Interrupt mask register
#define RTL8139_ISR         0x3E    // Interrupt status register
#define RTL8139_TCR         0x40    // Transmit configuration register
#define RTL8139_RCR         0x44    // Receive configuration register
#define RTL8139_CONFIG1     0x52    // Configuration register 1

// RTL8139 commands
#define RTL8139_CR_RST      0x10    // Reset
#define RTL8139_CR_RE       0x08    // Receiver enable
#define RTL8139_CR_TE       0x04    // Transmitter enable

// RTL8139 interrupt bits
#define RTL8139_ISR_ROK     0x01    // Receive OK
#define RTL8139_ISR_TOK     0x04    // Transmit OK
#define RTL8139_ISR_TER     0x08    // Transmit error
#define RTL8139_ISR_RER     0x02    // Receive error

// RTL8139 buffer sizes
#define RTL8139_TX_BUFFER_SIZE  1536
#define RTL8139_RX_BUFFER_SIZE  8192 + 16

// Ethernet device structure
typedef struct {
    uint16_t io_base;
    mac_addr_t mac_addr;
    uint8_t* rx_buffer;
    uint8_t* tx_buffer;
    uint32_t rx_buffer_pos;
    uint32_t tx_buffer_pos;
    int initialized;
} rtl8139_device_t;

// Ethernet functions
int rtl8139_init(uint16_t io_base);
int rtl8139_send_packet(const void* data, uint32_t len);
int rtl8139_recv_packet(void* buffer, uint32_t len);
void rtl8139_interrupt_handler(void);
mac_addr_t rtl8139_get_mac(void);

// Low-level register access
static inline void rtl8139_write8(uint16_t reg, uint8_t value) {
    outb(RTL8139_BASE + reg, value);
}

static inline void rtl8139_write16(uint16_t reg, uint16_t value) {
    outw(RTL8139_BASE + reg, value);
}

static inline void rtl8139_write32(uint16_t reg, uint32_t value) {
    outl(RTL8139_BASE + reg, value);
}

static inline uint8_t rtl8139_read8(uint16_t reg) {
    return inb(RTL8139_BASE + reg);
}

static inline uint16_t rtl8139_read16(uint16_t reg) {
    return inw(RTL8139_BASE + reg);
}

static inline uint32_t rtl8139_read32(uint16_t reg) {
    return inl(RTL8139_BASE + reg);
}

#endif // ETH_H