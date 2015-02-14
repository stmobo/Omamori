// irq.h
#pragma once
#include "includes.h"

extern "C" {
    extern void do_irq(size_t,size_t,size_t);
    extern bool irq_add_handler(irq_num_t, irq_handler);
    extern bool irq_remove_handler(irq_num_t, irq_handler);
    extern void block_for_irq(irq_num_t);
    extern bool in_irq_context;
}

extern void irq_set_mask(irq_num_t,bool);
extern bool irq_get_mask(irq_num_t);
extern void set_all_irq_status(bool);
void irq_end_interrupt( irq_num_t );
bool irq_get_in_service(irq_num_t );
