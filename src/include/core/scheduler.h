#pragma once
#include "includes.h"
#include "arch/x86/multitask.h"
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

struct process_ptr;

enum struct process_state {
    runnable,
    waiting,
    forking,
    dead,
};

typedef struct page_table {
    phys_addr_t paddr = NULL;
    int      flags = 0;
    int      pde_no = -1;
    bool     ready = false; // is true if the constructor was able to allocate all required resources.
    int      n_entries = 0;
    
    virt_addr_t map_addr = NULL;
    
    virt_addr_t map();   // this function allocates a page of virtual memory to use for modifying the page table.
    void unmap();   // this function deallocates the page allocated earlier with map_addr.
    page_table();   // Allocates space for the page table.
    ~page_table();  // Deallocates space for the page table.
} page_table; 

typedef struct address_space { // implementation in arch/paging.cpp
	phys_addr_t                page_directory_physical = NULL; // paddr of the PD
    virt_addr_t                *page_directory = NULL;         // a pointer to the PD's vaddr
    vector<page_table*>     *page_tables = NULL;
    bool                    ready = false;
    
    void unmap_pde( int );
    void map_pde( int, phys_addr_t, int );
    bool map_new( virt_addr_t, int );
    bool map( virt_addr_t, phys_addr_t, int );
    void unmap( virt_addr_t );
    uint32_t get( virt_addr_t );
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
    //vector< message* >*            message_queue;
    //mutex                          message_queue_lock;
    uint32_t                       in_syscall = 0;
    uint32_t                       break_val = PROCESS_BREAK_START;
    vector< process* >             children;
    process_times                  times;
    char*                          message_waiting_on;
    
    mutex						   process_reference_lock;
    vector< process_ptr* >		   process_reflist;

    bool operator==( const process& rhs ) { return (rhs.id == this->id); };
    bool operator!=( const process& rhs ) { return (rhs.id != this->id); };
    
    ~process();
    process( process* );
    process( virt_addr_t entry_point, bool is_usermode, int priority, const char* name, void* args, int n_args );
    
    void add_reference( process_ptr* );
    void remove_reference( process_ptr* );

    int  wait();
    //bool send_message( message );
} process;

typedef class process_ptr {
	process* raw;
	bool invalidated;

public:
	void invalidate() { this->invalidated = true; };
	bool valid() { return ( !this->invalidated && (this->raw != NULL) ); };
	process* raw_ptr() { return this->raw; };

	operator process*() { return this->raw; };
	process& operator*() { return *this->raw; };
	process* operator->() { return this->raw; };
	process_ptr& operator=(process_ptr& rhs);
	process_ptr& operator=(process*& rhs);

	~process_ptr();
	process_ptr( const process_ptr& );
	process_ptr( process* );
	process_ptr() : raw(NULL), invalidated(true) {};

} process_ptr;

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
