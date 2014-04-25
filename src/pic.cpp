// pic.cpp
#include "pic.h"
#include "sys.h"

void pic_end_interrupt(bool send_to_slave) {
    if(send_to_slave)
        io_outb(SLAVE_PIC_BASE, PIC_CMD_END_INTERRUPT);
    io_outb(MASTER_PIC_BASE, PIC_CMD_END_INTERRUPT);
}

void pic_initialize(char vector_offset_1, char vector_offset_2) {
    // ICW1 - tell PICs to go to initialization mode
    io_outb(MASTER_PIC_BASE, PIC_CMD_INITIALIZE);
    io_wait();
    io_outb(SLAVE_PIC_BASE, PIC_CMD_INITIALIZE);
    io_wait();
    // ICW2 - tell PICs their vector offsets
    io_outb(MASTER_PIC_BASE+1, vector_offset_1);
    io_wait();
    io_outb(SLAVE_PIC_BASE+1, vector_offset_2);
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

void irq_mask(unsigned char irq, bool set) {
    short port = MASTER_PIC_BASE;
    char mask;
    if(irq > 7) {
        port = SLAVE_PIC_BASE;
        irq -= 8;
    }
    mask = io_inb(port+1);
    if(set)
        mask |= (1<<irq);
    else
        mask &= (1<<irq);
    io_outb(port+1, mask);
    io_wait();
}

bool get_irq_mask_status(unsigned char irq) {
    short port = MASTER_PIC_BASE;
    char mask;
    if(irq > 7) {
        port = SLAVE_PIC_BASE;
        irq -= 8;
    }
    mask = io_inb(port+1);
    return mask & ~(1<<irq);
}

unsigned short get_irq_mask_all() {
    return io_inb(MASTER_PIC_BASE+1) | ( io_inb(SLAVE_PIC_BASE+1)<<8 );
}

void set_irq_mask_all(unsigned short mask) {
    unsigned char master = mask & 0xFF;
    unsigned char slave = mask & (0xFF<<8);
    io_outb(MASTER_PIC_BASE+1, master);
    io_outb(SLAVE_PIC_BASE+1, slave);
}

void set_all_irq_status(bool status) {
    if(status) {
        io_outb(MASTER_PIC_BASE+1, 0xFF);
        io_wait();
        io_outb(SLAVE_PIC_BASE+1, 0xFF);
    } else {
        io_outb(MASTER_PIC_BASE+1, 0);
        io_wait();
        io_outb(SLAVE_PIC_BASE+1, 0);
    }
    io_wait();
}