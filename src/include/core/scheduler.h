#pragma once
#include "includes.h"
#include "arch/x86/multitask.h"

#define SCHEDULER_PRIORITY_LEVELS       16
// stack size in pages
#define PROCESS_STACK_SIZE              4

enum struct process_state {
    runnable,
    waiting,
};

typedef struct event {
    const char *event_type = NULL;
    void **event_data = NULL;
    int  n_event_data = 0;
} event;

typedef struct process {
    cpu_regs    regs;
    cpu_regs    user_regs;
    bool        switched_from_syscall;
    uint32_t    id;
    uint32_t    parent;
    int         priority;
    uint32_t    *process_pde;
    page_frame  *frames_allocated;
    vaddr_range vmem_allocator;
    process_state   state;
    char*       event_wait = NULL;
    event*      event_data;
    
    
    process( cpu_regs, int );
    process( size_t, bool, int );
} process;

// simple FIFO process queue
typedef class process_list {
    process **queue;
    int length;
    int count;
    
    public:
    void add( process* );
    process* remove();
    process* operator[](int);
    int get_count() { return this->count; };
    process_list();
} process_list;


extern process *process_current;
extern process **system_processes;
extern int process_count;

extern void initialize_multitasking( process* );
extern void multitasking_start_init();
extern void process_scheduler();
extern void process_queue_add( process*, int );
extern process *shift_process_queue(process**, int);
extern void send_event(event*);
extern event* wait_for_event(const char*);
extern void process_add_to_runqueue( process* );