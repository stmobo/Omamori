// scheduler.cpp

#include "includes.h"
#include "arch/x86/table.h"
#include "arch/x86/multitask.h"
#include "core/scheduler.h"
#include "core/paging.h"

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
        if( this->queue != NULL ) {
            for(int i=0;i<this->count;i++) {
                new_queue[i] = this->queue[i];
            }
            delete this->queue;
        }
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
    if(this->address_space.ready) { // no point in setting anything else if the address space couldn't be allocated
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
        this->regs.esp = (0xC0000000-1);
    
        if( is_usermode ){
            size_t k_stack_start = mmap(4);
            if( k_stack_start == NULL ) {
                panic("multitasking: failed to allocate kernel stack frames for process!\n");
            }
            
            this->regs.kernel_stack = k_stack_start + (PROCESS_STACK_SIZE*0x1000);
            this->regs.cs = GDT_UCODE_SEGMENT*0x08;
            this->regs.ds = GDT_UDATA_SEGMENT*0x08;
            this->regs.es = GDT_UDATA_SEGMENT*0x08;
            this->regs.fs = GDT_UDATA_SEGMENT*0x08;
            this->regs.gs = GDT_UDATA_SEGMENT*0x08;
            this->regs.ss = GDT_UDATA_SEGMENT*0x08;
        } else {
            this->regs.kernel_stack = 0; // don't need to worry about TSS' esp0 / ss0 if this is a kmode process
            this->regs.cs = GDT_KCODE_SEGMENT*0x08;
            this->regs.ds = GDT_KDATA_SEGMENT*0x08;
            this->regs.es = GDT_KDATA_SEGMENT*0x08;
            this->regs.fs = GDT_KDATA_SEGMENT*0x08;
            this->regs.gs = GDT_KDATA_SEGMENT*0x08;
            this->regs.ss = GDT_KDATA_SEGMENT*0x08;
        }
    
        // Each process' stack runs from 0xBFFFFFFF to 0xBFFFC000 -- that's 0x3FFF bytes, or 1 byte shy of 16KB.
        // kernel mapping (recursive mapping's done in the address_space constructor)
        this->address_space.map_pde( 0, (size_t)&PageTable0, 1 );
        this->address_space.map_pde( 768, (size_t)&PageTable768, 1 );
        // map stack frames in
        for(int i=0;i<PROCESS_STACK_SIZE;i++) {
            this->address_space.map( ((0xC0000000-1)-(i*0x1000))&0xFFFFF000, 1 );
            kprintf("Set PTE for 0x%x to 0x%x.\n", (unsigned long long int)(((0xC0000000-1)-(i*0x1000))&0xFFFFF000), (unsigned long long int)(this->address_space.get(((0xC0000000-1)-(i*0x1000))&0xFFFFF000)));
        }
        this->regs.cr3 = this->address_space.page_directory_physical;
        kprintf("Loaded process CR3 as pmem 0x%x, vmem 0x%x.\n", (unsigned long long int)(this->regs.cr3), (unsigned long long int)(this->address_space.page_directory));
    } else {
        panic("multitasking: failed to initialize address space for process!\n");
    }
}

page_table::page_table() {
    page_frame *frame = pageframe_allocate(1);
    
    if( frame != NULL ) {
        this->ready = true;
        this->paddr = frame->address;
    }
}

page_table::~page_table() {
    pageframe_deallocate_specific( pageframe_get_block_from_addr( this->paddr ), 0 );
    if( this->map_addr != NULL ) {
        this->unmap();
    }
    this->ready = false;
}

size_t page_table::map() {
    size_t vaddr = k_vmem_alloc(1);
    if( __sync_bool_compare_and_swap( &this->map_addr, NULL, vaddr ) ) {
        system_disable_interrupts();
        paging_set_pte(vaddr, this->paddr, 0);
        system_enable_interrupts();
        return vaddr;
    }
    k_vmem_free(vaddr);
    return this->map_addr;
}

void page_table::unmap() {
    bool int_on = interrupts_enabled();
    system_disable_interrupts();
    
    if( this->map_addr != NULL ) {
        paging_unset_pte( this->map_addr );
        k_vmem_free( this->map_addr );
        this->map_addr = NULL;
    }
    
    if( int_on )
        system_enable_interrupts();
}

address_space::address_space() {
    page_frame *pd_frame = pageframe_allocate(1);
    size_t pd_vaddr = k_vmem_alloc(1);
    
    if( (pd_frame != NULL) && (pd_vaddr != 0) ) {
        paging_set_pte( pd_vaddr, pd_frame->address, 0 );
        this->page_directory_physical = pd_frame->address;
        this->page_directory = (uint32_t*)pd_vaddr;
        this->page_directory[1023] = this->page_directory_physical | 1;
        this->page_tables = NULL;
        this->n_page_tables = 0;
        this->ready = true;
    }
}

address_space::~address_space() {
    pageframe_deallocate_specific( pageframe_get_block_from_addr( this->page_directory_physical ), 0 );
    k_vmem_free( (size_t)this->page_directory );
    if( this->page_tables != NULL ) {
        kfree(this->page_tables);
        this->ready = false;
    }
}

void address_space::unmap_pde( int table_no ) {
    uint32_t *pde = (uint32_t*)((size_t)this->page_directory + (table_no*4));
    (*pde) = 0;
}

void address_space::map_pde( int table_no, size_t paddr, int flags ) {
    uint32_t *pde = (uint32_t*)((size_t)this->page_directory + (table_no*4));
    (*pde) = paddr | flags;
}

bool address_space::map( size_t vaddr, int flags ) {
    // map a given vaddr to an empty pageframe
    page_frame *frame = pageframe_allocate(1);
    if( frame != NULL ) {
        return this->map( vaddr, frame->address, flags );
    }
    return false;
}

bool address_space::map( size_t vaddr, size_t paddr, int flags ) {
    if(vaddr > 0xC0000000)
        return false; // Can't map things into kernel space from here
    flags &= 0xFFF;
    vaddr &= 0xFFFFF000;
    paddr &= 0xFFFFF000;
    
    int table_no = (vaddr >> 22);
    int table_offset = (vaddr >> 12) & 0x3FF;
    uint32_t *pde = (uint32_t*)((size_t)this->page_directory + (table_no*4));
    page_table* pt = NULL;
    //kprintf("address_space::map: checking for PDE at 0x%x.\n", (unsigned long long int)pde);
    // first, check to see if there's an actual page table for the page we want to map in
    // if not, then make a new one
    // also, find the page table's corresponding descriptor object
    if( ((*pde) & 1) == 0 ) {
        page_table *new_pt = new page_table;
        new_pt->pde_no = table_no;
        this->page_tables = extend<page_table*>(this->page_tables, this->n_page_tables);
        this->page_tables[this->n_page_tables] = new_pt;
        this->n_page_tables++;
        pt = new_pt;
        (*pde) = new_pt->paddr | 1;
    } else {
        for(int i=0;i<this->n_page_tables;i++) {
            if( this->page_tables[i]->pde_no == table_no ) {
                pt = this->page_tables[i];
                break;
            }
        }
        if( pt == NULL ) {
            // Remove the faulty PDE and retry the request
            (*pde) = 0;
            return this->map( vaddr, paddr, flags );
        }
    }
    
    uint32_t *table = (uint32_t*)pt->map();
    //kprintf("address_space::map: Page table at 0x%x mapped to 0x%x.\n", (unsigned long long int)pt->paddr, (unsigned long long int)table);
    table[table_offset] = paddr | flags;
    pt->unmap();
    return true;
}

void address_space::unmap( size_t vaddr ) {
    if(vaddr > 0xC0000000)
        return;
    vaddr &= 0xFFFFF000;
    
    int table_no = (vaddr >> 22);
    int table_offset = (vaddr >> 12) & 0x3FF;
    uint32_t *pde = (uint32_t*)((size_t)this->page_directory + (table_no*4));
    page_table* pt = NULL;

    if( ((*pde) & 1) == 0 ) {
        return;
    } else {
        for(int i=0;i<this->n_page_tables;i++) {
            if( this->page_tables[i]->pde_no == table_no ) {
                pt = this->page_tables[i];
                break;
            }
        }
        if( pt == NULL ) {
            // The address is not even mapped in in the first place, but clear out the faulty PDE anyways
            (*pde) = 0;
            return;
        }
    }
    
    uint32_t *table = (uint32_t*)pt->map();
    table[table_offset] = 0;
    pt->unmap();
}

uint32_t address_space::get( size_t vaddr ) {
    if(vaddr > 0xC0000000)
        return 0; // Can't map things into kernel space from here
    vaddr &= 0xFFFFF000;
    
    int table_no = (vaddr >> 22);
    int table_offset = (vaddr >> 12) & 0x3FF;
    uint32_t *pde = (uint32_t*)((size_t)this->page_directory + (table_no*4));
    page_table* pt = NULL;

    if( ((*pde) & 1) == 0 ) {
        return 0;
    } else {
        for(int i=0;i<this->n_page_tables;i++) {
            if( this->page_tables[i]->pde_no == table_no ) {
                pt = this->page_tables[i];
                break;
            }
        }
        if( pt == NULL ) {
            // The address is not even mapped in in the first place, but clear out the faulty PDE anyways
            (*pde) = 0;
            return 0;
        }
    }
    
    uint32_t *table = (uint32_t*)pt->map();
    uint32_t ret = table[table_offset];
    pt->unmap();
    return ret;
}