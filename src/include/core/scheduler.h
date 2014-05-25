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

typedef struct page_table {
    uint32_t paddr;
    int      flags;
    int      pde_no;
    bool     ready = false;
    
    size_t map_addr;
    
    size_t map();
    void unmap();
    page_table();
    ~page_table();
} page_table;

typedef struct address_space {
    uint32_t   page_directory_physical; // paddr of the PD
    uint32_t   *page_directory;         // a pointer to the PD's vaddr
    page_table **page_tables;
    int         n_page_tables;
    bool        ready = false;
    
    void unmap_pde( int );
    void map_pde( int, size_t, int );
    bool map( size_t, int );
    bool map( size_t, size_t, int );
    void unmap( size_t );
    uint32_t get( size_t );
    address_space();
    ~address_space();
} process_address_space;

typedef struct process {
    cpu_regs                 regs;
    cpu_regs                 user_regs;
    uint32_t                 id;
    uint32_t                 parent;
    int                      priority;
    process_address_space    address_space;
    process_state            state;
    char*                    event_wait = NULL;
    event*                   event_data;
    
    
    process( cpu_regs, int );
    process( size_t, bool, int );
} process;

// simple FIFO process queue
typedef class process_queue {
    process **queue = NULL;
    int length;
    int count;
    
    public:
    void add( process* );
    process* remove();
    process* operator[](int);
    int get_count() { return this->count; };
    process_queue();
} process_queue;

extern process *process_current;
extern process **system_processes;
extern int process_count;

extern void initialize_multitasking( process* );
extern void multitasking_start_init();
extern void process_scheduler();
extern void send_event(event*);
extern event* wait_for_event(const char*);
extern void process_add_to_runqueue( process* );
extern process* get_process_by_pid( int );
extern void spawn_process( process* );