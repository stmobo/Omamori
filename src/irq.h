// irq.h
#pragma once

extern "C" {
    extern void do_irq(size_t,size_t,size_t);
    extern bool add_irq_handler(int, size_t);
    extern void block_for_interrupt(int);
}