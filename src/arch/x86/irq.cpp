// irq.cpp - IRQ handlers

#include "includes.h"
#include "core/sys.h"
#include "arch/x86/pic.h"
#include "arch/x86/irq.h"
#include "lib/vector.h"

// remove this line at some point (it's just for debugging)
#include "arch/x86/multitask.h"

vector<size_t> irq_handlers[16];
signed int waiting_for = -1;
bool do_wait = false;
bool in_irq_context = false;

bool in_irq7 = false;
bool in_irq15 = false;

void do_irq(size_t irq_num, size_t eip, size_t cs) {
    /*
    if(irq_num == waiting_for)
        waiting_for = -1;
    */
    if( irq_num == 7 ) {
        if( in_irq7 || ((pic_get_isr() & 0x80) == 0) ) { // check to see if IRQ7 is actually happening (and don't enter the IRQ7 routine twice)
            return;
        } else {
            in_irq7 = true;
        }
    }
    if( irq_num == 15 ) {
        if( in_irq15 || ((pic_get_isr() & 0x8000) == 0) ) { // same thing as above, but with irq15
            return;
        } else {
            in_irq15 = true;
        }
    }
    
    in_irq_context = true;
    /*
    if(irq_num != 0) {
    	irqsafe_kprintf("Handling irq: %u.\n", irq_num);
    }
    */
    if(irq_handlers[irq_num].length() > 0) {
        for( unsigned int i=0;i<irq_handlers[irq_num].length();i++ ) {
            bool(*handler)(void) = (bool(*)())(irq_handlers[irq_num].get(i)); // jump to the stored function pointer...
            if(handler()) {
                break; // irq's handled, we're done here
            }
        }
    }
    pic_end_interrupt(irq_num);
    if( irq_num == 7 ) {
        in_irq7 = false;
    }
    if( irq_num == 15 ) {
        in_irq15 = false;
    }
    in_irq_context = false;
    return;
}

bool irq_add_handler(int irq_num, size_t addr) {
    for( unsigned int i=0;i<irq_handlers[irq_num].length();i++ ) {
        if( irq_handlers[irq_num].get(i) == addr ) {
            return false;
        }
    }
    irq_handlers[irq_num].add(addr);
    irq_set_mask(irq_num, false);
    return true;
}

bool irq_remove_handler(int irq_num, size_t addr) {
    for( unsigned int i=0;i<irq_handlers[irq_num].length();i++ ) {
        if( irq_handlers[irq_num].get(i) == addr ) {
            irq_handlers[irq_num].remove(i);
        }
    }
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
