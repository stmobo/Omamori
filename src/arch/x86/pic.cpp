// pic.cpp
#include "includes.h"
#include "arch/x86/sys.h"
#include "core/sys.h"
#include "arch/x86/pic.h"

void pic_end_interrupt(int irq) {
    if(irq > 7)
        io_outb(SLAVE_PIC_BASE, PIC_CMD_END_INTERRUPT);
    io_outb(MASTER_PIC_BASE, PIC_CMD_END_INTERRUPT);
}

void pic_initialize(char vector_offset_1) {
    // ICW1 - tell PICs to go to initialization mode
    io_outb(MASTER_PIC_BASE, PIC_CMD_INITIALIZE);
    io_wait();
    io_outb(SLAVE_PIC_BASE, PIC_CMD_INITIALIZE);
    io_wait();
    // ICW2 - tell PICs their vector offsets
    io_outb(MASTER_PIC_BASE+1, vector_offset_1);
    io_wait();
    io_outb(SLAVE_PIC_BASE+1, vector_offset_1+8);
    io_wait();
    // ICW3
    io_outb(MASTER_PIC_BASE+1, 4);
    io_wait();
    io_outb(SLAVE_PIC_BASE+1, 2);
    io_wait();
    // tell PICs to go to 8086 mode
    io_outb(MASTER_PIC_BASE+1, 0x01);
    io_wait();
    io_outb(SLAVE_PIC_BASE+1, 0x01);
    io_wait();
    // Disable interrupts on the PIC so we can enable them on the CPU.
    io_outb(MASTER_PIC_BASE+1, 0xFF);
    io_wait();
    io_outb(SLAVE_PIC_BASE+1, 0xFF);
    io_wait();
    asm volatile("sti" : : : "memory");
}

uint16_t pic_get_mask() {
    return ( ((uint16_t)io_inb(SLAVE_PIC_BASE+1))<<8 ) | ((uint16_t)io_inb(MASTER_PIC_BASE+1));
}

void pic_set_mask(uint16_t mask) {
    io_outb(MASTER_PIC_BASE+1, mask & 0xFF);
    io_outb(SLAVE_PIC_BASE+1, ((mask>>8) & 0xFF));
}

uint16_t pic_get_isr() {
    io_outb(MASTER_PIC_BASE, PIC_CMD_READ_ISR);
    io_outb(SLAVE_PIC_BASE, PIC_CMD_READ_ISR);
    return ((((uint16_t)io_inb(SLAVE_PIC_BASE)) << 8) | io_inb(MASTER_PIC_BASE));
}

uint16_t pic_get_irr() {
    io_outb(MASTER_PIC_BASE, PIC_CMD_READ_IRR);
    io_outb(SLAVE_PIC_BASE, PIC_CMD_READ_IRR);
    return ((((uint16_t)io_inb(SLAVE_PIC_BASE)) << 8) | io_inb(MASTER_PIC_BASE));
}