/*
 * page_table.cpp
 *
 *  Created on: Jan 23, 2015
 *      Author: Tatantyler
 */

#include "includes.h"
#include "core/scheduler.h"
#include "arch/x86/sys.h"

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
