// pit.h
#pragma once
#include "includes.h"

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

class timer_wait { 
    signed int ticks_left;
    
    public:
    bool in_use;
    signed int reload_val;
    bool repeating;
    bool active;
    void(*callback)(void);
    
    timer_wait();
    void decrement(int);
    void reload();
};

typedef class timer_wait timer_wait;

typedef class timer {
    timer_wait *internal;
    public:
    void set_callback(void(*)(void));
    void set_repeat(bool);
    void set_active(bool);
    void set_reload_val(int);
    bool get_repeat();
    bool get_active();
    int get_reload_val();
    void reload();
    timer(int,bool,bool,void(*)(void));
    ~timer();
} timer;

extern unsigned long long int get_sys_elapsed_time(void);
extern unsigned long long int get_sys_time_counter(void);
extern void pit_initialize(short reload_val);