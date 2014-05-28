#pragma once
#include "includes.h"
#include "arch/x86/multitask.h"
#include "device/pit.h"
#include "lib/vector.h"

#define SCHEDULER_PRIORITY_LEVELS       16
// stack size in pages
#define PROCESS_STACK_SIZE              4

#define PROCESS_FLAGS_DELETE_ON_EXIT    (1<<0)

extern "C" {
    extern void process_exec_complete(uint32_t);
    extern void __process_execution_complete(void);
}

enum struct process_state {
    runnable,
    waiting,
    dead,
};

typedef struct page_table {
    uint32_t paddr = NULL;
    int      flags = 0;
    int      pde_no = -1;
    bool     ready = false; // is true if the constructor was able to allocate all required resources.
    
    size_t map_addr = NULL;
    
    size_t map();   // this function allocates a page of virtual memory to use for modifying the page table.
    void unmap();   // this function deallocates the page allocated earlier with map_addr.
    page_table();   // Allocates space for the page table.
    ~page_table();  // Deallocates space for the page table.
} page_table; 

typedef struct address_space {
    uint32_t   page_directory_physical = NULL; // paddr of the PD
    uint32_t   *page_directory = NULL;         // a pointer to the PD's vaddr
    page_table **page_tables = NULL;
    int         n_page_tables = 0;
    bool        ready = false;
    
    void unmap_pde( int );
    void map_pde( int, size_t, int );
    bool map_new( size_t, int );
    bool map( size_t, size_t, int );
    void unmap( size_t );
    uint32_t get( size_t );
    address_space();
    ~address_space();
} process_address_space;

typedef struct message {
    const char* message_name;
    void*       data;
    uint64_t    timestamp;
    
    message( const char* ev_name, void* ev_data ) { this->message_name = ev_name; this->data = ev_data; this->timestamp = get_sys_time_counter(); };
    message();
} message;

typedef struct process {
    cpu_regs                 regs;
    cpu_regs                 user_regs;
    uint32_t                 id;
    uint32_t                 parent;
    const char*              name;
    uint32_t                 flags;
    int                      priority;
    process_address_space    address_space;
    process_state            state;
    uint32_t                 wait_time;
    vector<const char*>      wait_events;
    uint32_t                 return_value;
    vector<message*>         message_queue;
    
    ~process();
    process( cpu_regs, int, const char* );
    process( size_t entry_point, bool is_usermode, int priority, const char* name, void* args, int n_args );
    
    bool send_message( message );
    
    void set_message_filter_varg( int, va_list );
    void set_message_filter( int, ... );
    
    void add_to_message_filter_varg( int, va_list );
    void add_to_message_filter( int, ... );
    
    void remove_from_message_filter_varg( int, va_list );
    void remove_from_message_filter( int, ... );
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

// messaging stuff
extern message *wait_for_message( int, ... );
extern message *wait_for_message();
extern message *get_latest_message();
extern int send_message_all( message msg );

// process stuff
extern void process_scheduler();
extern void process_add_to_runqueue( process* );
extern process* get_process_by_pid( int );
extern void spawn_process( process*, bool=true );

extern "C" {
    extern void process_switch_immediate();
}