// pit.cpp - Programmable Interval Timer chip
// aka IRQ0

#include "includes.h"
#include "arch/x86/irq.h"
#include "device/vga.h"
#include "device/pit.h"
#include "lib/linked_list.h"

// note to self: write the PIT initialization code at some point

unsigned long long int tick_counter = 0;

unsigned int master_reload_value = 0;
double pit_frequency = 0; // in KHz.

unsigned long long int sys_timer_ms = 0;
double sys_timer_ms_fraction = 0;
double ms_per_tick = 0; // (1/pit_frequency)*1000
linked_list<timer_wait> active_timers(NULL,NULL);

timer_wait::timer_wait() {
    this->ticks_left = 0;
    this->reload_val = 0;
    this->callback = NULL;
    this->repeating = false;
    this->active = false;
    this->in_use = false;
}

// timer_wait::reload - reload the timer
void timer_wait::reload() {
    this->ticks_left = this->reload_val;
    this->active = true;
}

// timer_wait::decrement - Decrement the timer's value.
// If this->ticks_left goes <= 0 (i.e the timer has gone off) the alarm is marked as inactive and the optional
// callback is called. If there is a callback and the alarm is set to repeat, then the timer is reloaded and
// marked as active once more.
void timer_wait::decrement(int n) {
    if(this->ticks_left < n) {
        this->ticks_left = 0;
        this->active = false;
        /*
        if(this->callback != NULL) {
            this->callback();
            if(this->repeating) {
                this->reload();
            }
        }
        */
    } else {
        this->ticks_left -= n;
    }
}

// timer::<ctor> - allocate a timer_wait object and attach it to the new timer object
timer::timer(int reload_val, bool immediate_active, bool repeat, void(*callback)(void)) {
    // allocate a timer_wait object
    timer_wait *wait_obj = NULL;
    for(int i=0;i<active_timers.n_elements();i++) {
        if(!active_timers[i]->in_use) {
            wait_obj = active_timers[i];
        }
    }
    if(wait_obj == NULL) {
        active_timers.append();
        wait_obj = active_timers[active_timers.n_elements()-1];
    }
    wait_obj->callback = callback;
    wait_obj->repeating = repeat;
    wait_obj->reload_val = reload_val;
    wait_obj->active = !immediate_active;
    // now actually initialize the wrapper.
    this->internal = wait_obj;
    // now activate the timer.
    wait_obj->in_use = true;
}

timer::~timer() {
    this->internal->in_use = false;
}

// bunch of self-explanatory getter/setter functions
void timer::set_callback(void(*callback)(void)) {
    this->internal->callback = callback;
}

void timer::set_repeat(bool repeat) {
    this->internal->repeating = repeat;
}

void timer::set_active(bool active) {
    this->internal->active = active;
}

void timer::set_reload_val(int reload) {
    this->internal->reload_val = reload;
}

bool timer::get_repeat() {
    return this->internal->repeating;
}

bool timer::get_active() {
    return this->internal->active;
}

int timer::get_reload_val() {
    return this->internal->reload_val;
}

void timer::reload() {
    this->internal->reload();
}

void irq0_handler() {
    //terminal_writestring("IRQ0!\n");
    tick_counter++;
    sys_timer_ms_fraction += ms_per_tick;
    int ms_added = floor(sys_timer_ms_fraction);
    
    /*
    terminal_writestring("Milliseconds added:");
    terminal_writestring(int_to_decimal(ms_added));
    */
    sys_timer_ms += ms_added;
    sys_timer_ms_fraction = fractional(sys_timer_ms_fraction);
    
    /*
    terminal_writestring("\nNew systimer:");
    terminal_writestring(int_to_decimal(sys_timer_ms));
    */
    for(int i=0;i<active_timers.n_elements();i++) {
        /*
        terminal_writestring("\ni=");
        terminal_writestring(int_to_decimal(i));
        terminal_writestring("\n");
        */
        if(active_timers[i]->in_use) {
            //terminal_writestring("hurrr");
            active_timers[i]->decrement(ms_added);
        }
    }
}

unsigned long long int get_sys_elapsed_time() {
    return tick_counter;
}

unsigned long long int get_sys_time_counter() {
    return sys_timer_ms;
}

void set_pit_reload_val(short reload_val) {
    disable_interrupts();
    io_outb(0x43, (3<<1) | (3<<3)); // set PIT command reg. to channel 0, sequential hi/lo byte access
    io_outb(0x40, (reload_val & 0xFF));
    io_outb(0x40, (reload_val >> 8)&0xFF);
    enable_interrupts();
}

void pit_initialize(short reload_val) {
    pit_frequency = PIT_BASE_TIMER_SIGNAL / reload_val;
    ms_per_tick = (1/pit_frequency);
    set_pit_reload_val(reload_val);
    irq_add_handler(0, (size_t)&irq0_handler);
}