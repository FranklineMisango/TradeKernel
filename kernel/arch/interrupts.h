#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "../types.h"

// IDT entry structure
typedef struct {
    uint16_t offset_low;    // Lower 16 bits of handler address
    uint16_t selector;      // Code segment selector
    uint8_t zero;          // Must be zero
    uint8_t type_attr;     // Type and attributes
    uint16_t offset_high;   // Upper 16 bits of handler address
} __attribute__((packed)) idt_entry_t;

// IDT descriptor
typedef struct {
    uint16_t limit;        // Size of IDT - 1
    uint32_t base;         // Address of IDT
} __attribute__((packed)) idt_descriptor_t;

// Interrupt frame (pushed by CPU during interrupt)
typedef struct {
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
    uint32_t esp;
    uint32_t ss;
} interrupt_frame_t;

// Function prototypes
void interrupts_init(void);
void set_idt_entry(int num, uint32_t handler, uint16_t selector, uint8_t flags);

// Interrupt handlers
void keyboard_handler(void);
void timer_handler(void);
void page_fault_interrupt_handler(void);

// External page fault handler (from paging.c)
extern void page_fault_handler(uint32_t error_code, uint32_t virtual_addr);

// Assembly wrappers (defined in interrupt_handlers.asm)
extern void keyboard_interrupt_wrapper(void);
extern void timer_interrupt_wrapper(void);
extern void page_fault_interrupt_wrapper(void);
extern void syscall_interrupt_handler(void);

// Timing functions
uint32_t get_ticks(void);

#endif // INTERRUPTS_H