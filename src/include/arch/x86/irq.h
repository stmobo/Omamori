// irq.h
#pragma once
#include "includes.h"

typedef bool(*irq_handler)(uint8_t);

extern "C" {
    extern void do_irq(size_t,size_t,size_t);
    extern bool irq_add_handler(int, irq_handler);
    extern bool irq_remove_handler(int, irq_handler);
    extern void block_for_irq(int);
    extern bool in_irq_context;
}

extern void irq_set_mask(unsigned char,bool);
extern bool irq_get_mask(unsigned char);
extern void set_all_irq_status(bool);
void irq_end_interrupt( unsigned int irq_num );
bool irq_get_in_service( unsigned int irq_num );
