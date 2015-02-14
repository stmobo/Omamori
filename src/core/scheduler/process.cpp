/*
 * process.cpp
 *
 *  Created on: Jan 23, 2015
 *      Author: Tatantyler
 */

#include "includes.h"
#include "core/scheduler.h"
#include "arch/x86/sys.h"
#include "arch/x86/table.h"
#include "arch/x86/multitask.h"

extern uint32_t allocate_new_pid();
extern vector<process*> run_queues[SCHEDULER_PRIORITY_LEVELS];

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
    	this->process_reference_lock.lock(); // keep people from getting references to us

    	this->state = process_state::dead;

    	for( unsigned int i=0;i<this->process_reflist.count();i++ ) {
    		this->process_reflist[i]->invalidate();
    	}

        for( unsigned int i=0;i<system_processes.count();i++ ) {
            if( (system_processes[i] != NULL) && (system_processes[i]->id == this->id) ) {
                system_processes.set( i, NULL );
                break;
            }
        }
        for( unsigned int i=0;i<this->parent->children.count();i++ ) {
            if( (this->parent->children[i] != NULL) && (this->parent->children[i]->id == this->id) ) {
                this->parent->children.set( i, NULL );
                break;
            }
        }
        for( unsigned int i=0;i<run_queues[this->priority].count();i++ ) {
            if( run_queues[this->priority][i]->id == this->id ) {
                run_queues[this->priority].remove(i);
            }
        }

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
    //this->message_queue = new vector<message*>;
    //kprintf("process::process - exiting!\n");
}

// notes for debugging:
// the stack PDE (and the kernel PDE) should start at vaddr 0xFFFFFBFC.
// the stack page table should start at vaddr 0xFFEFF000.
// the kernel page table should start at vaddr 0xFFF00000.
process::process( virt_addr_t entry_point, bool is_usermode, int priority, const char* name, void* args, int n_args ) {
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
        //this->message_queue = new vector<message*>;
    } else {
        panic("multitasking: failed to initialize address space for process!\n");
    }
}
