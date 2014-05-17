// irq.cpp - IRQ handlers
// aka "A demonstration in what NOT to do with size_t's and pointers"
// then again, it's pretty safe.

#include "includes.h"
#include "core/sys.h"
#include "device/pic.h"
#include "device/irq.h"

size_t irq_handlers[16] = { NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };
signed int waiting_for = -1;
bool do_wait = false;

void do_irq(size_t irq_num, size_t eip, size_t cs) {
    /*
    if(irq_num == waiting_for)
        waiting_for = -1;
    */
    if(irq_handlers[irq_num] != NULL) {
        void (*handler)(void) = (void(*)())irq_handlers[irq_num]; // jump to the stored function pointer...
        handler(); // ...and hope there's someone there to catch us.
    }
    if(irq_num > 7)
        pic_end_interrupt(true);
    else
        pic_end_interrupt(false);
    return;
}

bool add_irq_handler(int irq_num, size_t addr) {
    if(irq_handlers[irq_num] != NULL)
        return false;
    irq_handlers[irq_num] = addr;
    irq_mask(irq_num, false);
    return true;
}

void block_for_interrupt(int irq) {
    if((irq < 0) || (irq > 15))
        return;
    waiting_for = irq;
    unsigned short mask = get_irq_mask_all();
    set_all_irq_status(true);
    irq_mask(irq, false);
    while(true) {
        wait_for_interrupt();
        if(waiting_for == -1)
            break;
    }
    set_irq_mask_all(mask);
}