// irq.cpp - IRQ handlers
// aka "A demonstration in what NOT to do with size_t's and pointers"
// then again, it's pretty safe.

#include "includes.h"
#include "core/sys.h"
#include "arch/x86/pic.h"
#include "arch/x86/irq.h"

size_t irq_handlers[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
signed int waiting_for = -1;
bool do_wait = false;
bool in_irq_context = false;

void do_irq(size_t irq_num, size_t eip, size_t cs) {
    /*
    if(irq_num == waiting_for)
        waiting_for = -1;
    */
    in_irq_context = true;
    if(irq_handlers[irq_num] != 0) {
        void (*handler)(void) = (void(*)())irq_handlers[irq_num]; // jump to the stored function pointer...
        handler(); // ...and hope there's someone there to catch us.
    }
    pic_end_interrupt(irq_num);
    in_irq_context = false;
    return;
}

bool irq_add_handler(int irq_num, size_t addr) {
    if(irq_handlers[irq_num] != 0)
        return false;
    irq_handlers[irq_num] = addr;
    irq_set_mask(irq_num, false);
    return true;
}

bool irq_remove_handler(int irq_num) {
    if(irq_handlers[irq_num] == 0)
        return false;
    irq_handlers[irq_num] = 0;
    irq_set_mask(irq_num, true);
    return true;
}

void block_for_irq(int irq) {
    if((irq < 0) || (irq > 15))
        return;
    waiting_for = irq;
    unsigned short mask = pic_get_mask();
    set_all_irq_status(true);
    irq_set_mask(irq, false);
    while(true) {
        system_wait_for_interrupt();
        if(waiting_for == -1)
            break;
    }
    pic_set_mask(mask);
}

void irq_set_mask(unsigned char irq, bool set) {
    uint16_t mask = pic_get_mask();
    if( set ) {
        mask |= (1<<irq);
    } else {
        mask &= ~(1<<irq);
    }
    pic_set_mask(mask);
}

bool irq_get_mask(unsigned char irq) {
    uint16_t mask = pic_get_mask();
    return ((mask & (1<<irq)) > 0);
}

void set_all_irq_status(bool status) {
    if(status) {
        pic_set_mask(0xFFFF);
    } else {
        pic_set_mask(0);
    }
}