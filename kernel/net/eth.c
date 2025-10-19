#include "eth.h"
#include "../mm/memory.h"
#include "../drivers/vga.h"

// Global RTL8139 device
static rtl8139_device_t rtl8139_dev;

// Initialize RTL8139 Ethernet controller
int rtl8139_init(uint16_t io_base) {
    vga_write_string("Initializing RTL8139 Ethernet controller...\n");

    rtl8139_dev.io_base = io_base;
    rtl8139_dev.initialized = 0;

    // Allocate RX and TX buffers
    rtl8139_dev.rx_buffer = (uint8_t*)kmalloc(RTL8139_RX_BUFFER_SIZE);
    rtl8139_dev.tx_buffer = (uint8_t*)kmalloc(RTL8139_TX_BUFFER_SIZE);

    if (!rtl8139_dev.rx_buffer || !rtl8139_dev.tx_buffer) {
        vga_write_string("Failed to allocate RTL8139 buffers\n");
        return NET_ERROR;
    }

    // Reset the device
    rtl8139_write8(RTL8139_CR, RTL8139_CR_RST);
    while (rtl8139_read8(RTL8139_CR) & RTL8139_CR_RST) {
        // Wait for reset to complete
    }

    // Read MAC address
    for (int i = 0; i < 6; i++) {
        rtl8139_dev.mac_addr.addr[i] = rtl8139_read8(RTL8139_IDR0 + i);
    }

    vga_write_string("RTL8139 MAC address: ");
    char* mac_str = net_mac_to_string(&rtl8139_dev.mac_addr);
    vga_write_string(mac_str);
    vga_write_string("\n");

    // Set receive buffer
    rtl8139_write32(RTL8139_RBSTART, (uint32_t)rtl8139_dev.rx_buffer);

    // Configure receive buffer
    rtl8139_write32(RTL8139_RCR, 0x0000000F); // Accept all packets

    // Enable transmitter and receiver
    rtl8139_write8(RTL8139_CR, RTL8139_CR_RE | RTL8139_CR_TE);

    // Configure transmit configuration
    rtl8139_write32(RTL8139_TCR, 0x00000300); // Max DMA burst size

    rtl8139_dev.rx_buffer_pos = 0;
    rtl8139_dev.tx_buffer_pos = 0;
    rtl8139_dev.initialized = 1;

    vga_write_string("RTL8139 Ethernet controller initialized\n");
    return NET_SUCCESS;
}

// Send a packet
int rtl8139_send_packet(const void* data, uint32_t len) {
    if (!rtl8139_dev.initialized || len > ETH_MTU) {
        return NET_ERROR;
    }

    // Wait for previous transmission to complete
    while (!(rtl8139_read32(RTL8139_TSD0) & (1 << 15))) {
        // Wait for TOK (Transmit OK)
    }

    // Copy packet to TX buffer
    memcpy(rtl8139_dev.tx_buffer, data, len);

    // Set transmit start address
    rtl8139_write32(RTL8139_TSAD0, (uint32_t)rtl8139_dev.tx_buffer);

    // Set transmit status descriptor (size and ownership)
    rtl8139_write32(RTL8139_TSD0, len);

    return NET_SUCCESS;
}

// Receive a packet
int rtl8139_recv_packet(void* buffer, uint32_t len) {
    if (!rtl8139_dev.initialized) {
        return NET_ERROR;
    }

    // Check if packet is available
    uint16_t status = rtl8139_read16(RTL8139_ISR);
    if (!(status & RTL8139_ISR_ROK)) {
        return 0; // No packet available
    }

    // Get packet size
    uint32_t packet_size = rtl8139_read16(RTL8139_CBR);

    if (packet_size > len) {
        return NET_ERROR; // Buffer too small
    }

    // Copy packet from RX buffer
    uint32_t rx_pos = rtl8139_dev.rx_buffer_pos;
    memcpy(buffer, rtl8139_dev.rx_buffer + rx_pos, packet_size);

    // Update buffer position
    rtl8139_dev.rx_buffer_pos = (rx_pos + packet_size + 4) % RTL8139_RX_BUFFER_SIZE;

    // Update CAPR (Current Address of Packet Read)
    rtl8139_write16(RTL8139_CAPR, rtl8139_dev.rx_buffer_pos - 16);

    // Clear interrupt
    rtl8139_write16(RTL8139_ISR, RTL8139_ISR_ROK);

    return packet_size;
}

// Get MAC address
mac_addr_t rtl8139_get_mac(void) {
    return rtl8139_dev.mac_addr;
}

// Interrupt handler (placeholder for now)
void rtl8139_interrupt_handler(void) {
    uint16_t status = rtl8139_read16(RTL8139_ISR);

    if (status & RTL8139_ISR_ROK) {
        // Packet received
        rtl8139_write16(RTL8139_ISR, RTL8139_ISR_ROK);
    }

    if (status & RTL8139_ISR_TOK) {
        // Packet transmitted
        rtl8139_write16(RTL8139_ISR, RTL8139_ISR_TOK);
    }

    if (status & RTL8139_ISR_TER) {
        // Transmit error
        rtl8139_write16(RTL8139_ISR, RTL8139_ISR_TER);
    }

    if (status & RTL8139_ISR_RER) {
        // Receive error
        rtl8139_write16(RTL8139_ISR, RTL8139_ISR_RER);
    }
}