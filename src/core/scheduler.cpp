// scheduler.cpp

#include "includes.h"
#include "arch/x86/table.h"
#include "arch/x86/multitask.h"
#include "core/scheduler.h"
#include "core/paging.h"
#include "device/pit.h"

// scheduling priorities have 0 as the highest priority

process *process_current = NULL;
vector<process*> system_processes;

uint32_t current_pid = 2; // 0 is reserved for the kernel (in the "parent" field only) and 1 is used for the initial process, which has a special startup sequence.
bool pids_have_overflowed = false;

process_queue run_queues[SCHEDULER_PRIORITY_LEVELS];
process_queue sleep_queue;

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

process* process_queue::remove( int n ) {
    if((this->count <= n) || (this->length <= n)) {
        return NULL;
    }
    process *shift_out = this->queue[n];
    for(int i=n;i<(this->count-1);i++) {
        this->queue[i] = this->queue[i+1];
    }
    this->queue[(this->count)-1] = NULL;
    this->count -= 1;
    return shift_out;
}

process* process_queue::remove() {
    return this->remove(0);
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

void process_add_to_runqueue( process* process_to_add ) {
    if( process_to_add == NULL )
        return;
    if( (process_to_add->priority < SCHEDULER_PRIORITY_LEVELS) && (process_to_add->priority >= 0) ) {
        for( int i=0;i<run_queues[process_to_add->priority].get_count();i++ ) {
            if( run_queues[process_to_add->priority][i] && (run_queues[process_to_add->priority][i]->id == process_to_add->id) ) {
                return; // already added
            }
        }
        process_to_add->state = process_state::runnable;
        run_queues[process_to_add->priority].add( process_to_add );
    }
}

void process_scheduler() {
    asm volatile("cli" : : : "memory");
    int current_priority = -1;
    for( int i=0;i<SCHEDULER_PRIORITY_LEVELS;i++ ) {
        if( run_queues[i].get_count() > 0 ) {
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

int process::wait() {
    if( this->flags & PROCESS_FLAGS_DELETE_ON_EXIT ) {
        this->flags &= ~(PROCESS_FLAGS_DELETE_ON_EXIT);
    }
    while(true) {
        if( this->state == process_state::dead ) {
            return this->return_value;
        }
        process_switch_immediate();
    }
}

process::~process() {
    if( this->id != 0 ) {
        for( unsigned int i=0;i<system_processes.length();i++ ) {
            if( (system_processes[i] != NULL) && (system_processes[i]->id == this->id) ) {
                system_processes.set( i, NULL );
                break;
            }
        }
        for( unsigned int i=0;i<this->parent->children.length();i++ ) {
            if( (this->parent->children[i] != NULL) && (this->parent->children[i]->id == this->id) ) {
                this->parent->children.set( i, NULL );
                break;
            }
        }
        for( int i=0;i<run_queues[this->priority].get_count();i++ ) {
            if( run_queues[this->priority][i]->id == this->id ) {
                run_queues[this->priority].remove(i);
            }
        }
        this->state = process_state::dead;
        if( process_current->id == this->id ) {
            this->user_regs.eip = (uint32_t)&__process_execution_complete;
            this->regs.eip = (uint32_t)&__process_execution_complete; // just in case we happen to come back
        }
    }
}

process::process( process* forked_process ) {
    this->user_regs = forked_process->user_regs;
    this->regs = forked_process->regs;
    this->priority = forked_process->priority;
    this->name = forked_process->name;
    this->id = allocate_new_pid();
    this->parent = forked_process;
    
    // traverse the process' page directory, and duplicate frames that aren't in PTs 768 or 0.
    // this, coincidentally, also copies the stack.
    this->address_space.page_tables = new vector<page_table*>;
    //kprintf("process::process - process has %u page tables.\n", forked_process->address_space.page_tables->length());
    for( unsigned int pt_num=0;pt_num<(forked_process->address_space.page_tables->length());pt_num++ ) {
        // PDEs that were mapped in manually using address_space::map_pde() aren't copied here.
        // They're not in forked_process->page_tables.
        //kprintf("process::process - copying page table ID %u.\n", pt_num);
        if( forked_process->address_space.page_tables->get(pt_num)->n_entries > 0 ) {
            page_table *current = forked_process->address_space.page_tables->get(pt_num);
            //kprintf("process::process - copying page table %u.\n", current->pde_no);
            uint32_t *pde = (uint32_t*)((size_t)this->address_space.page_directory + (current->pde_no*4));
            page_table *dest_pt = new page_table;
            
            //kprintf("process::process - allocated new page table.\n");
            
            if(!dest_pt->ready)
                panic("fork: failed to create copy of page table for process!");
            
            //kprintf("process::process - mapping tables into memory.\n");
            
            uint32_t* current_vaddr = (uint32_t*)current->map();
            uint32_t* dest_vaddr = (uint32_t*)dest_pt->map();
            
            if( (current_vaddr == NULL) || (dest_vaddr == NULL) )
                panic("fork: failed to allocate source / destination page for process!");
            
            //kprintf("process::process - copying PTEs...\n");
            for(int pte_num=0;pte_num<1024;pte_num++) {
                uint32_t pframe = current_vaddr[pte_num] & 0xFFFFF000;
                uint16_t flags =  current_vaddr[pte_num] & 0x00000FFF;
                if( current_vaddr[pte_num] != 0 ) {
                    page_frame* dframe = duplicate_pageframe_range( pframe, 1 );
                    dest_vaddr[pte_num] = dframe->address | flags;
                    delete dframe;
                }
            }
            //kprintf("process::process - done copying page table.\n");
            
            current->unmap();
            dest_pt->unmap();
            (*pde) = dest_pt->paddr | 1;
            this->address_space.page_tables->add(dest_pt);
            //kprintf("process::process - added PT to PD / vector.\n");
        }
    }
    
    //kprintf("process::process - mapping in special PDEs.\n");
    // kernel mappings, same as below
    this->address_space.map_pde( 0, (size_t)&PageTable0, 1 );
    this->address_space.map_pde( 768, (size_t)&PageTable768, 1 );
    
    // need to update cr3 to point to our new PD.
    this->user_regs.cr3 = this->address_space.page_directory_physical;
    this->regs.cr3 = this->address_space.page_directory_physical;
    this->message_queue = new vector<message*>;
    //kprintf("process::process - exiting!\n");
}

// notes for debugging:
// the stack PDE (and the kernel PDE) should start at vaddr 0xFFFFFBFC.
// the stack page table should start at vaddr 0xFFEFF000.
// the kernel page table should start at vaddr 0xFFF00000.
process::process( size_t entry_point, bool is_usermode, int priority, const char* name, void* args, int n_args ) {
    if(this->address_space.ready) { // no point in setting anything else if the address space couldn't be allocated
        this->id = allocate_new_pid();
        this->parent = process_current;
        this->state = process_state::runnable;
        this->priority = priority;
        this->name = name;
        this->regs.eax = 0;
        this->regs.ebx = 0;
        this->regs.ecx = 0;
        this->regs.edx = 0;
        this->regs.esi = 0;
        this->regs.edi = 0;
        this->regs.eip = entry_point;
        this->regs.eflags = (1<<9) | (1<<21); // Interrupt Flag and Identification Flag
        
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
            //kprintf("multitasking: mapping in stack page for new process: 0x%x\n", (unsigned long long int)(((0xC0000000-1)-(i*0x1000))&0xFFFFF000));
            if(!this->address_space.map_new( ((0xC0000000-1)-(i*0x1000))&0xFFFFF000, 1 ))
                panic("multitasking: failed to initialize stack frames for process!");
        }
        uint32_t stack_phys_page = this->address_space.get( 0xBFFFF000 ) & 0xFFFFF000;
        uint32_t* tmp_stack_page = (uint32_t*)k_vmem_alloc(1);
        if( stack_phys_page == NULL )
            panic("multitasking: failed to initialize stack frames!");
        //kprintf("multitasking: process stack starts at paddr 0x%x.\n", (unsigned long long int)stack_phys_page);
        paging_set_pte( (size_t)tmp_stack_page, stack_phys_page, 0 );
        tmp_stack_page[1022] = n_args;
        tmp_stack_page[1021] = (uint32_t)args;
        tmp_stack_page[1020] = (uint32_t)&__process_execution_complete;
        paging_unset_pte( (size_t)tmp_stack_page );
        this->regs.ebp = 0xBFFFFFF0; // 0xC0000000 - 4 - 4 - 4 - 4 
        this->regs.esp = 0xBFFFFFF0; // 0xC0000000 - 4 - 4 - 4 - 4
        this->regs.cr3 = this->address_space.page_directory_physical;
        this->message_queue = new vector<message*>;
    } else {
        panic("multitasking: failed to initialize address space for process!\n");
    }
}

address_space::address_space() {
    page_frame *pd_frame = pageframe_allocate(1);
    size_t pd_vaddr = k_vmem_alloc(1);
    
    if( (pd_frame != NULL) && (pd_vaddr != 0) ) {
        paging_set_pte( pd_vaddr, pd_frame->address, 0 );
        this->page_directory_physical = pd_frame->address;
        this->page_directory = (uint32_t*)pd_vaddr;
        this->page_directory[1023] = this->page_directory_physical | 1;
        this->page_tables = new vector<page_table*>;
        this->ready = true;
    }
}

address_space::~address_space() {
    if(this->ready) {
        pageframe_deallocate_specific( pageframe_get_block_from_addr( this->page_directory_physical ), 0 );
        k_vmem_free( (size_t)this->page_directory );
        if( this->page_tables != NULL ) {
            for(unsigned int i=0;i<this->page_tables->length();i++) {
                if( this->page_tables->get(i)->n_entries > 0 ) {
                    uint32_t *pt = (uint32_t*)this->page_tables->get(i)->map();
                    for(int j=0;j<1024;j++) {
                        size_t paddr = pt[i] & 0xFFFFF000;
                        if( paddr != 0 ) {
                            int frameID = pageframe_get_block_from_addr( paddr );
                            pageframe_deallocate_specific( frameID, 0 );
                        }
                    }
                    this->page_tables->get(i)->unmap();
                }
                delete (this->page_tables->get(i));
            }
            delete this->page_tables;
        }
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

bool address_space::map_new( size_t vaddr, int flags ) {
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
        //kprintf("address_space::map - Creating new page table.\n");
        page_table *new_pt = new page_table;
        if( !new_pt->ready )
            return false;
        new_pt->pde_no = table_no;
        this->page_tables->add_end(new_pt);
        /*
        this->page_tables = extend<page_table*>(this->page_tables, this->n_page_tables);
        if( !this->page_tables )
            return false;
        this->page_tables[this->n_page_tables] = new_pt;
        this->n_page_tables++;
        */
        pt = new_pt;
        (*pde) = new_pt->paddr | 1;
    } else {
        for(unsigned int i=0;i<this->page_tables->length();i++) {
            if( this->page_tables->get(i)->pde_no == table_no ) {
                pt = this->page_tables->get(i);
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
    if( table ) {
        //kprintf("address_space::map: Page table at 0x%x mapped to 0x%x.\n", (unsigned long long int)pt->paddr, (unsigned long long int)table);
        //kprintf("address_space::map: mapping v0x%x -> p0x%x.\n", (unsigned long long int)vaddr, (unsigned long long int)paddr );
        //kprintf("address_space::map: table=0x%p, table_offset=%u\n", table, (unsigned long long int)table_offset );
        if( table[table_offset] == 0 )
            pt->n_entries++;
        table[table_offset] = paddr | flags;
        pt->unmap();
        return true;
    }
    return false;
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
        for(unsigned int i=0;i<page_tables->length();i++) {
            if( this->page_tables->get(i)->pde_no == table_no ) {
                pt = this->page_tables->get(i);
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
    int block_addr = pageframe_get_block_from_addr( table[table_offset] & 0xFFFFF000 );
    if( block_addr != -1 ) {
        pageframe_deallocate_specific(block_addr, 0);
    }
    if( table[table_offset] != 0 ) {
        pt->n_entries--;
        table[table_offset] = 0;
        pt->unmap();
        if( pt->n_entries == 0 ) { // there's nothing here anymore, we can free it
            (*pde) = 0;
            delete pt;
        }
    } else {
        pt->unmap();
    }
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
        //kprintf("address_space::get: could not find PDE.\n");
        return 0;
    } else {
        for(unsigned int i=0;i<this->page_tables->length();i++) {
            if( this->page_tables->get(i)->pde_no == table_no ) {
                pt = this->page_tables->get(i);
                break;
            }
        }
        if( pt == NULL ) {
            // The address is not even mapped in in the first place, but clear out the faulty PDE anyways
            //kprintf("address_space::get: could not find PDE (removed faulty PDE).\n");
            (*pde) = 0;
            return 0;
        }
    }
    
    uint32_t *table = (uint32_t*)pt->map();
    if( table ) {
        //kprintf("address_space::get: table=0x%p, table_offset=%u\n", table, (unsigned long long int)table_offset );
        uint32_t ret = table[table_offset];
        //kprintf("address_space::get: mapping seems to be v0x%x -> p0x%x.\n", (unsigned long long int)vaddr, (unsigned long long int)ret );
        pt->unmap();
        return ret;
    }
    //kprintf("address_space::get: could not map page table to active memory.\n");
    return 0;
}

page_table::page_table() {
    page_frame *frame = pageframe_allocate(1);
    
    if( frame != NULL ) {
        this->ready = true;
        this->paddr = frame->address;
        //kprintf("New page table initialized at address 0x%x.\n", (uint64_t)this->paddr);
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
    if( vaddr == NULL )
        return NULL;
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