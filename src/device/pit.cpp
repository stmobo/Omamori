// pit.cpp - Programmable Interval Timer chip
// aka IRQ0

#include "includes.h"
#include "arch/x86/irq.h"
#include "arch/x86/multitask.h"
#include "core/scheduler.h"
#include "device/vga.h"
#include "device/pit.h"

unsigned long long int tick_counter = 0;

unsigned int master_reload_value = 0;
double pit_frequency = 0; // in KHz.

unsigned long long int sys_timer_ms = 0;
double sys_timer_ms_fraction = 0;
double ms_per_tick = 0; // (1/pit_frequency)*1000

// This is called AFTER context switching, but before new task context is loaded.
bool irq0_handler() {
    /*
    if(multitasking_enabled) {
        kprintf("IRQ0!\nTimeslice counter: 0x%x!\n", (unsigned long long int)multitasking_timeslice_tick_count);
    }
    */
    tick_counter++;
    sys_timer_ms_fraction += ms_per_tick;
    int ms_added = floor(sys_timer_ms_fraction);

    sys_timer_ms += ms_added;
    sys_timer_ms_fraction = fractional(sys_timer_ms_fraction);
    
    if( multitasking_enabled && (process_current != NULL) ) {
        if( process_current->in_syscall != 0 )
            process_current->times.sysc_exec++;
        else
            process_current->times.prog_exec++;
    }
    
    return true;
}

unsigned long long int get_sys_elapsed_time() {
    return tick_counter;
}

unsigned long long int get_sys_time_counter() {
    return sys_timer_ms;
}

void set_pit_reload_val(short reload_val) {
    system_disable_interrupts();
    io_outb(0x43, (3<<1) | (3<<3)); // set PIT command reg. to channel 0, sequential hi/lo byte access
    io_outb(0x40, (reload_val & 0xFF));
    io_outb(0x40, (reload_val >> 8)&0xFF);
    system_enable_interrupts();
}

void pit_initialize(short reload_val) {
    pit_frequency = PIT_BASE_TIMER_SIGNAL / reload_val;
    ms_per_tick = (1/pit_frequency);
    set_pit_reload_val(reload_val);
    irq_add_handler(0, (size_t)&irq0_handler);
}