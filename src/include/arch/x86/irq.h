// irq.h
#pragma once
#include "includes.h"

extern "C" {
    extern void do_irq(size_t,size_t,size_t);
    extern bool irq_add_handler(int, size_t);
    extern void block_for_irq(int);
}

extern void irq_set_mask(unsigned char,bool);
extern bool irq_get_mask(unsigned char);
extern void set_all_irq_status(bool);