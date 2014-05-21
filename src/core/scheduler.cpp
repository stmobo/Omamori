// scheduler.cpp

#include "includes.h"
#include "arch/x86/table.h"
#include "arch/x86/multitask.h"
#include "core/scheduler.h"
#include "core/paging.h"

extern uint32_t *PageTable0;
extern uint32_t *PageTable768;
extern uint32_t *BootPD; // see early_boot.s

// scheduling priorities have 0 as the highest priority

process *process_current = NULL;
process **system_processes = NULL;
int process_count = 0;

uint32_t current_pid = 2; // 0 is reserved for the kernel (in the "parent" field only) and 1 is used for the initial process, which has a special startup sequence.
bool pids_have_overflowed = false;

process_queue run_queues[SCHEDULER_PRIORITY_LEVELS];
process_queue sleep_queue;

uint32_t allocate_new_pid() {
    uint32_t ret = current_pid;
    current_pid++;
    if( current_pid == 0 ) { // overflow
        current_pid = 777;
        pids_have_overflowed = true;
    }
    if( pids_have_overflowed ) {
        bool is_okay = false;
        while( !is_okay ) {
            is_okay = true;
            for( int i=0;i<process_count;i++ ) {
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

process* get_process_by_pid( int pid ) {
    for( int i=0;i<process_count;i++ ) {
        if( system_processes[i]->id == pid ) {
            return system_processes[i];
        }
    }
    return NULL;
}

void spawn_process( process* to_add ) {
    if( process_current ) {
        to_add->parent = 0;
    } else {
        to_add->parent = process_current->id;
    }
    to_add->id = allocate_new_pid();
    process_add_to_runqueue( to_add );
    kprintf("Starting new process with ID: %u.", (unsigned long long int)to_add->id);
}

void process_queue::add(process* process_to_add) {
    if( this->count+1 > this->length ) {
        this->length += 1;
        process **new_queue = new process*[this->length];
        for(int i=0;i<this->count;i++) {
            new_queue[i] = this->queue[i];
        }
        delete this->queue;
        new_queue[this->length-1] = process_to_add;
        this->queue = new_queue;
    } else {
        this->queue[this->count] = process_to_add;
    }
    this->count += 1;
}

process* process_queue::remove() {
    if((this->count <= 0) || (this->length <= 0)) {
        return NULL;
    }
    process *shift_out = this->queue[0];
    for(int i=0;i<(this->count-1);i++) {
        this->queue[i] = this->queue[i+1];
    }
    this->count -= 1;
    return shift_out;
}

process* process_queue::operator[]( int n ) {
    if( (this->count < n) || (this->length < n) ) {
        return NULL;
    }
    return this->queue[n];
}

process_queue::process_queue() {
    this->queue = NULL;
    this->length = 0;
    this->count = 0;
}

void send_event(event *ev) {
    for(int i=0;i<sleep_queue.get_count();i++) {
        process *iter = sleep_queue.remove();
        if( strcmp(iter->event_wait, const_cast<char*>(ev->event_type), 0) ) {
            iter->event_data = new event;
            *(iter->event_data) = *ev;
            process_add_to_runqueue(iter);
        } else {
            sleep_queue.add(iter);
        }
    }
}

event* wait_for_event(const char* event_name) {
    if( process_current != NULL ) {
        process_current->state = process_state::waiting;
        strcpy( process_current->event_wait, const_cast<char*>(event_name), 0 );
        sleep_queue.add( process_current );
        process_switch_immediate();
        return process_current->event_data;
    }
    return NULL;
}

void process_add_to_runqueue( process* process_to_add ) {
    if( (process_to_add->priority < SCHEDULER_PRIORITY_LEVELS) && (process_to_add->priority >= 0) ) {
        process_to_add->state = process_state::runnable;
        run_queues[process_to_add->priority].add( process_to_add );
    } else {
        kprintf("Invalid process priority %u\n", (unsigned long long int)(process_to_add->priority));
    }
}

void process_scheduler() {
    int current_priority = -1;
    for( int i=0;i<SCHEDULER_PRIORITY_LEVELS;i++ ) {
        if( run_queues[i].get_count() > 0 ) {
            current_priority = i;
            //kprintf("Scheduling process of priority %u.\n", (unsigned long long int)i);
            break;
        }
    }
    if(current_priority == -1) {
        system_wait_for_interrupt();
        return process_scheduler();
    }
    
    process_current = run_queues[current_priority].remove();
    //kprintf("New pid=%u.\n", (unsigned long long int)process_current->id);
}

// caller is responsible for ensuring that the register values are correct!
process::process( cpu_regs regs, int priority ) {
    this->regs = regs;
    this->priority = priority;
}

process::process( size_t entry_point, bool is_usermode, int priority ) {
    size_t stack_start = k_vmem_alloc(PROCESS_STACK_SIZE);
    if(stack_start == NULL) {
        panic("multitasking: failed to allocate new kernel stack for process!\n");
    }
    
    this->id = 0;
    this->parent = 0;
    this->state = process_state::runnable;
    this->priority = priority;
    this->regs.eax = 0;
    this->regs.ebx = 0;
    this->regs.ecx = 0;
    this->regs.edx = 0;
    this->regs.esi = 0;
    this->regs.edi = 0;
    this->regs.eip = entry_point;
    this->regs.eflags = (1<<9) | (1<<21); // Interrupt Flag and Identification Flag
    this->regs.ebp = 0;
    this->regs.kernel_stack = stack_start + (PROCESS_STACK_SIZE*0x1000);
    if( is_usermode ){
        this->regs.esp = (uint32_t)(0xC00000000-1); // if the stack is underrun, then a protection violation happens.
        this->regs.cs = GDT_UCODE_SEGMENT*0x08;
        this->regs.ds = GDT_UDATA_SEGMENT*0x08;
        this->regs.es = GDT_UDATA_SEGMENT*0x08;
        this->regs.fs = GDT_UDATA_SEGMENT*0x08;
        this->regs.gs = GDT_UDATA_SEGMENT*0x08;
        this->regs.ss = GDT_UDATA_SEGMENT*0x08;
        
        size_t pde_page = k_vmem_alloc(1);
        page_frame *pde_frame = pageframe_allocate(1);
        
        if((pde_frame != NULL) && (pde_page != NULL)) {
            paging_set_pte( pde_page, pde_frame->address, 0 );
            this->process_pde = (uint32_t*)pde_page;
            
            kprintf("Process PD mapped to vmem 0x%x / pmem 0x%x\n", (unsigned long long int)pde_page, (unsigned long long int)pde_frame->address);
            
            // do kernel mapping:
            this->process_pde[0] = ((uint32_t)&PageTable0) | 1; // Kernelmode, Read-Only, Present
            this->process_pde[768] = ((uint32_t)&PageTable768) | 1; // Kernelmode, Read-Only, Present
            this->process_pde[1023] = ((uint32_t)pde_frame->address) | 7; // bits: Usermode, Read/Write, Present
            this->regs.cr3 = pde_frame->address;
        } else {
            panic("multitasking: failed to allocate new page directory for process!\n");
        }
    } else {
        this->regs.esp = (stack_start + (PROCESS_STACK_SIZE*0x1000))-4;
        page_frame *stack_frames = pageframe_allocate(PROCESS_STACK_SIZE);
        for(int i=0;i<PROCESS_STACK_SIZE;i++) {
            paging_set_pte( stack_start + (i*0x1000), stack_frames[i].address, 0 );
        }
        kfree( (char*)(stack_frames) );
        this->regs.cs = GDT_KCODE_SEGMENT*0x08;
        this->regs.ds = GDT_KDATA_SEGMENT*0x08;
        this->regs.es = GDT_KDATA_SEGMENT*0x08;
        this->regs.fs = GDT_KDATA_SEGMENT*0x08;
        this->regs.gs = GDT_KDATA_SEGMENT*0x08;
        this->regs.ss = GDT_KDATA_SEGMENT*0x08;
        this->regs.cr3 = (size_t)&BootPD; // use kernel page structures for simplicity
    }
}