// pit.h
#pragma once
#include "includes.h"
#include "core/scheduler.h"

// PIT input signal runs at 1.193182 MHz.
// this #define is for the input signal in KHz.
#define PIT_BASE_TIMER_SIGNAL       1193.182f
#define PIT_DEFAULT_FREQ_DIVISOR    0x04A8

/*
timer_wait objects (see pit.cpp) are stored in a vector, also located in pit.cpp.

In order to provide for the RAII idiom and for space efficiency, we provide a separate wrapper class called "timer".
These classes "reserve" an element in the vector. Upon destruction, another timer object can then reserve the now
freed timer_wait object.
*/

typedef struct timer {
	uint64_t n_ticks;
	unsigned int id;
} timer;

extern unsigned long long int get_sys_elapsed_time(void);
extern unsigned long long int get_sys_time_counter(void);
extern void pit_initialize(short reload_val);

void timer_initialize();
unsigned int start_timer( uint64_t n_ticks );
