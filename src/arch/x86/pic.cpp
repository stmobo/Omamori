// pic.cpp
#include "includes.h"
#include "arch/x86/sys.h"
#include "core/sys.h"
#include "arch/x86/pic.h"
#include "core/device_manager.h"

bool pic_8259_initialized;

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
    pic_8259_initialized = true;
    asm volatile("sti" : : : "memory");

    device_manager::device_node* dev = new device_manager::device_node;
	dev->child_id = device_manager::root.children.count();
	dev->enabled = true;
	dev->type = device_manager::dev_type::ioapic;

	device_manager::device_resource* res = new device_manager::device_resource;
	res->consumes = true;
	res->type = device_manager::res_type::io_port;
	res->io_port.start = MASTER_PIC_BASE;
	res->io_port.end = MASTER_PIC_BASE+1;

	dev->resources.add_end(res);

	res = new device_manager::device_resource;
	res->consumes = true;
	res->type = device_manager::res_type::io_port;
	res->io_port.start = SLAVE_PIC_BASE;
	res->io_port.end = SLAVE_PIC_BASE+1;

	dev->resources.add_end(res);

	for(unsigned int i=0;i<16;i++) {
		res = new device_manager::device_resource;
		res->consumes = false;
		res->type = device_manager::res_type::interrupt;
		res->interrupt = i;

		dev->resources.add_end(res);
	}

	device_manager::root.children.add_end( dev );
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
