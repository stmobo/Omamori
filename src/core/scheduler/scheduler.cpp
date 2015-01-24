/*
 * scheduler.cpp
 *
 *  Created on: Jan 23, 2015
 *      Author: Tatantyler
 */

#include "includes.h"
#include "core/scheduler.h"

process *process_current = NULL;
vector<process*> system_processes;

uint32_t current_pid = 2; // 0 is reserved for the kernel (in the "parent" field only) and 1 is used for the initial process, which has a special startup sequence.
bool pids_have_overflowed = false;

vector<process*> run_queues[SCHEDULER_PRIORITY_LEVELS];
vector<process*> sleep_queue;

uint32_t allocate_new_pid() {
    uint32_t ret = current_pid++;
    if( current_pid == 0 ) { // overflow
        current_pid = 777;
        pids_have_overflowed = true;
    }
    if( pids_have_overflowed ) {
        bool is_okay = false;
        while( !is_okay ) {
            is_okay = true;
            for( unsigned int i=0;i<system_processes.length();i++ ) {
                if( (system_processes[i]->id) == ret ) {
                    ret++;
                    is_okay = false;
                    break;
                }
            }
        }
    }
    return ret;
}

process* get_process_by_pid( unsigned int pid ) {
    for( unsigned int i=0;i<system_processes.length();i++ ) {
        if( (system_processes[i]) && (system_processes[i]->id == pid) ) {
            return system_processes[i];
        }
    }
    return NULL;
}

void spawn_process( process* to_add, bool sched_immediate ) {
    system_processes.add( to_add );
    if( sched_immediate )
        process_add_to_runqueue( to_add );
    //kprintf("Starting new process with ID: %u (%s).", (unsigned long long int)to_add->id, to_add->name);
}

// syscall implementation
semaphore __debug_fork_sema(1,1);
uint32_t do_fork() {
    process* child_process = new process( process_current );
    if( child_process != NULL ) {
        process_current->children.add_end(child_process);
        process_current->user_regs.eax = child_process->id;        // so we need to move the new regs over
        spawn_process( child_process, false );
        process_add_to_runqueue( process_current );                // schedule ourselves to run later
        process_current->state = process_state::forking;           // tell the scheduler to load from user_regs
        child_process->state = process_state::runnable;            // let the child run
        process_current = child_process;
        return 0;
    }
    return -1;
}

void process_add_to_runqueue( process* process_to_add ) {
    if( process_to_add == NULL )
        return;
    if( (process_to_add->priority < SCHEDULER_PRIORITY_LEVELS) && (process_to_add->priority >= 0) ) {
        for( int i=0;i<run_queues[process_to_add->priority].count();i++ ) {
            if( run_queues[process_to_add->priority][i] && (run_queues[process_to_add->priority][i]->id == process_to_add->id) ) {
                return; // already added
            }
        }
        process_to_add->state = process_state::runnable;
        run_queues[process_to_add->priority].add_end( process_to_add );
    }
}

void process_sleep() {
	process_current->state = process_state::waiting;
	process_switch_immediate();
}

void process_wake(process* proc) {
	proc->state = process_state::runnable;
	process_add_to_runqueue(proc);
}

bool is_valid_process( process* proc ) {
	for(unsigned int i=0;i<system_processes.count();i++) {
		if( system_processes[i] == proc )
			return true;
	}
	return false;
}

void process_scheduler() {
    asm volatile("cli" : : : "memory");
    int current_priority = -1;
    for( int i=0;i<SCHEDULER_PRIORITY_LEVELS;i++ ) {
        if( run_queues[i].count() > 0 ) {
            current_priority = i;
            //kprintf("Scheduling process of priority %u.\n", (unsigned long long int)i);
            break;
        }
    }
    if(current_priority == -1) {
        multitasking_enabled = false; // don't jump to the context switch handler on IRQ0
        //kprintf("scheduler: no available processes left, sleeping.\n");
        asm volatile("sti" : : : "memory"); // make sure we actually can wake up from this
        system_wait_for_interrupt(); // sleep for a bit
        multitasking_enabled = true;
        return process_scheduler();
    }

    process_current = run_queues[current_priority].remove();
    if( process_current->state == process_state::dead ) {
        if( process_current->flags & PROCESS_FLAGS_DELETE_ON_EXIT ) {
            delete process_current;
            process_current = NULL;
        }
        return process_scheduler();
    }
    //kprintf("New pid=%u.\n", (unsigned long long int)process_current->id);
}
