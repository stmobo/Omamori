#pragma once
#include "includes.h"
#include "arch/x86/multitask.h"
#include "core/message.h"
#include "device/pit.h"
#include "lib/vector.h"

#define SCHEDULER_PRIORITY_LEVELS       16
// stack size in pages
#define PROCESS_STACK_SIZE              4

#define PROCESS_FLAGS_DELETE_ON_EXIT    (1<<0)

// size in cpu_reg structs
// total size = PROCESS_REG_STACK_SIZE*sizeof(cpu_reg)
#define PROCESS_REG_STACK_SIZE          5

#define PROCESS_BREAK_START             0x40000000

extern "C" {
    extern void process_exec_complete(uint32_t);
    extern void __process_execution_complete(void);
}

enum struct process_state {
    runnable,
    waiting,
    forking,
    dead,
};

typedef struct page_table {
    uint32_t paddr = NULL;
    int      flags = 0;
    int      pde_no = -1;
    bool     ready = false; // is true if the constructor was able to allocate all required resources.
    int      n_entries = 0;
    
    size_t map_addr = NULL;
    
    size_t map();   // this function allocates a page of virtual memory to use for modifying the page table.
    void unmap();   // this function deallocates the page allocated earlier with map_addr.
    page_table();   // Allocates space for the page table.
    ~page_table();  // Deallocates space for the page table.
} page_table; 

typedef struct address_space { // implementation in arch/paging.cpp
    uint32_t                page_directory_physical = NULL; // paddr of the PD
    uint32_t                *page_directory = NULL;         // a pointer to the PD's vaddr
    vector<page_table*>     *page_tables = NULL;
    bool                    ready = false;
    
    void unmap_pde( int );
    void map_pde( int, size_t, int );
    bool map_new( size_t, int );
    bool map( size_t, size_t, int );
    void unmap( size_t );
    uint32_t get( size_t );
    address_space();
    ~address_space();
} process_address_space;

typedef struct process_times {
    uint32_t prog_exec;
    uint32_t sysc_exec;
} process_times;

typedef struct process {
    cpu_regs                       regs;
    cpu_regs                       user_regs;
    uint32_t                       id;
    process*                       parent;
    const char*                    name;
    uint32_t                       flags;
    int                            priority;
    process_address_space          address_space;
    process_state                  state;
    uint32_t                       wait_time;
    uint32_t                       return_value;
    vector< message* >*            message_queue;
    mutex                          message_queue_lock;
    uint32_t                       in_syscall = 0;
    uint32_t                       break_val = PROCESS_BREAK_START;
    vector< process* >             children;
    process_times                  times;
    char*                          message_waiting_on;
    
    bool operator==( const process& rhs ) { return (rhs.id == this->id); };
    bool operator!=( const process& rhs ) { return (rhs.id != this->id); };
    
    ~process();
    process( process* );
    process( uint32_t entry_point, bool is_usermode, int priority, const char* name, void* args, int n_args );
    
    int  wait();
    bool send_message( message );
} process;

// simple FIFO process queue
typedef class process_queue {
    process **queue = NULL;
    int length;
    int count;
    
    public:
    void add( process* );
    process* remove();
    process* remove( int );
    process* operator[](int);
    int get_count() { return this->count; };
    process_queue();
} process_queue;

extern process *process_current;
extern vector<process*> system_processes;

// initialization stuff
extern void initialize_multitasking( process* );
extern void multitasking_start_init();

// process stuff
extern void process_scheduler();
extern void process_add_to_runqueue( process* );
extern process* get_process_by_pid( unsigned int );
extern void spawn_process( process* to_add, bool sched_immediate=true );
extern uint32_t do_fork();
extern bool is_valid_process();
extern void process_sleep();
extern void process_wake( process* );
extern "C" {
    extern uint32_t fork();
    extern void process_switch_immediate();
}
