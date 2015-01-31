// pit.cpp - Programmable Interval Timer chip
// aka IRQ0

#include "includes.h"
#include "arch/x86/sys.h"
#include "arch/x86/irq.h"
#include "arch/x86/multitask.h"
#include "core/scheduler.h"
#include "core/message.h"
#include "device/vga.h"
#include "device/pit.h"

unsigned long long int tick_counter = 0;

unsigned int master_reload_value = 0;
double pit_frequency = 0; // in KHz.

unsigned long long int sys_timer_ms = 0;
double sys_timer_ms_fraction = 0;
double ms_per_tick = 0; // (1/pit_frequency)*1000

unsigned int last_timer_id = 0;

vector< timer* > timers;
vector< timer* > finished_timers;
process* timer_worker_thread;

void timer_wake_thread() {
	while( true ) {
		if( finished_timers.count() > 0 ) {
			for(unsigned int i=0;i<finished_timers.count();i++) {
				message msg( finished_timers[i], sizeof(timer));
				send_to_channel( "timer", msg );
			}
			for(unsigned int i=0;i<finished_timers.count();i++) {
				delete finished_timers[i];
			}
			finished_timers.clear();
		} else {
			process_sleep();
		}
	}
}

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

        unsigned int offset = 0;

        if( timers.count() > 0 ) {
        	for(unsigned int i=0;i<timers.count();i++) {
        		if( timers[i-offset] != NULL ) {
        			timers[i-offset]->n_ticks--;
					if( timers[i-offset]->n_ticks == 0 ) {
						finished_timers.add_end(timers[i-offset]);
						timers.remove(i-offset);
						offset++;
						process_wake( timer_worker_thread );
					}
        		}
        	}
        }
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

void timer_initialize() {
	timer_worker_thread = new process( (uint32_t)&timer_wake_thread, false, 0, "k_timer_thread", NULL, 0 );
	register_channel( "timer" );
	spawn_process(timer_worker_thread);
}

unsigned int start_timer( uint64_t n_ticks ) {
	timer* t = new timer;
	t->n_ticks = n_ticks;
	t->id = last_timer_id++;

	timers.add_end( t );

	return t->id;
}
