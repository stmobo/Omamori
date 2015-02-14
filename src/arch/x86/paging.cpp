// page frame size is 4 kilobytes
// we're using a buddy allocator system for page frames.

// Our test system has 2GB, in 2096700 page frames.
// So, our order 0 bitmap is 256 KB.
// we have bitmaps for everything up to order 8 (1MB blocks).
// In total, our bitmaps will take up 512KB of space.

// We use a linked-list allocator for virtual memory in both kernel and user space.

#include "includes.h"
#include "boot/multiboot.h"
#include "core/paging.h"
#include "core/scheduler.h"
#include "device/vga.h"
#include "lib/sync.h"

/*
My test system says:
Avail: 
0x0 - 0x9FC00 (almost 640 KB - this is conventional memory)
0x100000 - 0x7FFF0000 (almost 2048MB - this is the rest of our memory)

Note the gap from 0x9FC00 - 0x100000 (spanning about 0x60400 bytes, or 385KB)
This is the EBDA / Vram / ROM area. We can't overwrite this area, or Bad Things happen.

In addition, what GRUB doesn't tell us is that memory from 0x0 to 0x500 is used by various things
and may be overwritten if we return to Real Mode for whatever reason.
(it's the RealMode IVT and the BIOS Data Area). We could theoretically overwrite it
but that's probably going to break stuff.

There are also a few other spots we need to keep marked as "always in memory".
One is the region from 0x10000 to kernel_end -- this is kernel code.
We also need to keep the buddy maps for the frame allocator in memory for obvious reasons.
*/

uint32_t initial_heap_pagetable[1024] __attribute__((aligned(0x1000)));
uint32_t global_kernel_page_directory[256]; // spans PDE nos. 768 - 1023

void paging_set_pte(size_t vaddr, size_t paddr, uint16_t flags) {
    if( vaddr < 0xC0000000 ) {
        process_current->address_space.map(vaddr, paddr, flags);
        return;
    }
    
    int table_no = (vaddr >> 22); // should always be >= 768
    int table_offset = (vaddr >> 12) & 0x3FF;
    uint32_t *pde = (uint32_t*)(0xFFFFF000 + (table_no*4));
    // first, check to see if there's an actual page table for the page we want to map in
    // if not, then make a new one (or load in the preexisting one)
    if( ((*pde) & 1) == 0 ) {
        if( global_kernel_page_directory[table_no-768] != 0 ) {
            (*pde) = global_kernel_page_directory[table_no-768];
        } else {
            page_frame* frame = pageframe_allocate(1);
            (*pde) = frame->address | 1;
            global_kernel_page_directory[table_no-768] = frame->address | 1;
        }
    }
    
    uint32_t *table = (uint32_t*)(0xFFC00000+(table_no*0x1000));
    uint32_t pte = table[table_offset];
    if( (pte & 1) > 0 ) {
        // okay, so there's already a mapping present for this page.
        // we need to swap it out, but we don't have a hard disk driver yet.
        // so right now we just let it be.
        //kprintf("paging: Attempted to map vaddr 0x%x when mapping already present!\n", (unsigned long long int)vaddr);
        //return; 
    }
    table[table_offset] = paddr | (flags & 0xFFF) | 1;
    invalidate_tlb( vaddr );
}

void paging_unset_pte(size_t vaddr) {
    if( vaddr < 0xC0000000 ) {
        return process_current->address_space.unmap(vaddr);
    }
    
    int table_no = (vaddr >> 22);
    int table_offset = (vaddr >> 12) & 0x3FF;
    uint32_t *pde = (uint32_t*)(0xFFFFF000 + (table_no*4));
    // first, check to see if there's an actual page table for the page we want to map in
    if( ((*pde) & 1) == 0 ) {
        if( global_kernel_page_directory[table_no-768] != 0 ) {
            // load this page table in to unmap globally
            (*pde) = global_kernel_page_directory[table_no-768];
        } else {
            // otherwise, this page isn't even mapped in the first place.
            return;
        }
    }
    
    uint32_t *table = (uint32_t*)(0xFFC00000+(table_no*0x1000));
    uint32_t pte = table[table_offset];
    if( (pte & 1) > 0 ) {
        table[table_offset] = 0;
        invalidate_tlb( vaddr );
    }
}

uint32_t paging_get_pte(size_t vaddr) {
    if( vaddr < 0xC0000000 ) {
        return process_current->address_space.get(vaddr);
    }
    
    int table_no = (vaddr >> 22);
    int table_offset = (vaddr >> 12) & 0x3FF;
    uint32_t *pde = (uint32_t*)(0xFFFFF000 + (table_no*4));
    // first, check to see if there's an actual page table for the page we want to map in
    // if not, then return "no such PTE"
    if( ((*pde) & 1) == 0 ) {
        if( global_kernel_page_directory[table_no-768] != 0 ) {
            // load this page table in
            (*pde) = global_kernel_page_directory[table_no-768];
        } else {
            // otherwise, this page isn't even mapped in the first place.
            return 0xFFFFFFFF;
        }
    }
    
    uint32_t *table = (uint32_t*)(0xFFC00000+(table_no*0x1000));
    uint32_t pte = table[table_offset];
    if( (pte & 1) == 0 ) {
        // no PTE for address
        return 0xFFFFFFFF;
    }
    return table[table_offset];
}


address_space::address_space() {
    page_frame *pd_frame = pageframe_allocate(1);
    size_t pd_vaddr = k_vmem_alloc(1);
    
    if( (pd_frame != NULL) && (pd_vaddr != 0) ) {
        paging_set_pte( pd_vaddr, pd_frame->address, 0 );
        this->page_directory_physical = pd_frame->address;
        this->page_directory = (virt_addr_t*)pd_vaddr;
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
    if(vaddr > 0xC0000000) {
        if( (process_current != NULL) && (process_current->id == 1) )
            kprintf("address_space[%u]::map -- attempted to map in kernel-space page\n", process_current->id);
        return false; // Can't map things into kernel space from here
    }
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
        if( !new_pt->ready ) {
            if( (process_current != NULL) )
                kprintf("address_space[%u]::map -- failed to allocate page table struct\n", process_current->id);
            return false;
        }
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
    
    if( (process_current != NULL) && (process_current->address_space.page_directory_physical == this->page_directory_physical) ) {
        int table_no = (vaddr >> 22);
        int table_offset = (vaddr >> 12) & 0x3FF;
        
        // if we're the currently loaded process, we can just modify the recursively-mapped tables.
        // and we don't need to check for the pde in this case (see above)
        
        uint32_t *table = (uint32_t*)(0xFFC00000+(table_no*0x1000));
        uint32_t pte = table[table_offset];
        if( (pte & 1) == 0 ) {
            table[table_offset] = (paddr & 0xFFFFF000) | flags | 1;
            invalidate_tlb( vaddr );
            return true;
        } else if( pte == ((paddr&0xFFFFF000) | flags) ) {
            // the PTE's already here.
            return true;
        }
        if( (process_current != NULL) )
            kprintf("address_space[%u]::map -- attempted to remap already present page %u to %u\n", process_current->id, vaddr, paddr);
    } else {
        uint32_t *table = (uint32_t*)pt->map();
        if( table ) {
            //kprintf("address_space::map: Page table at 0x%x mapped to 0x%x.\n", (unsigned long long int)pt->paddr, (unsigned long long int)table);
            //kprintf("address_space::map: mapping v0x%x -> p0x%x.\n", (unsigned long long int)vaddr, (unsigned long long int)paddr );
            //kprintf("address_space::map: table=0x%p, table_offset=%u\n", table, (unsigned long long int)table_offset );
            if( table[table_offset] == 0 )
                pt->n_entries++;
            table[table_offset] = paddr | flags | 1;
            pt->unmap();
            return true;
        }
        if( (process_current != NULL) )
            kprintf("address_space[%u]::map -- failed to map page table into virtual memory\n", process_current->id);
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

uint32_t panic_cr2;
uint32_t panic_ins;
uint32_t recursive_cr2;
uint32_t recursive_ins;
bool in_pagefault = false;
void paging_handle_pagefault(char error_code, uint32_t cr2, uint32_t eip, uint32_t cs) {
    if(in_pagefault) { // don't want to recursively pagefault (yet)
        recursive_cr2 = cr2;
        recursive_ins = eip;
        panic("paging: page fault in page fault handler!\npaging: initial CR2/ins: 0x%x, 0x%x\npaging: recursive CR2/ins: 0x%x, 0x%x", panic_cr2, panic_ins, recursive_cr2, recursive_ins);
    }
    panic_cr2 = cr2;
    panic_ins = eip;
    in_pagefault = true;
    if( (error_code & 1) == 0 ) {
        if( !pageframes_initialized ) {
            terminal_writestring("panic: paging: a page fault occured, but the allocator isn't ready yet!\n");
            while(true) {
                asm volatile("cli\n\t"
                "hlt\n\t"
                : : : "memory");
            }
        }
        if( cr2 < 0x1000 ) { // invalid address-- first page is not and never should be mapped (NULL address page)
            panic("paging: invalid memory access (possible NULL pointer dereference?)\nAccessed address: 0x%x\nEIP=0x%x\nCS=0x%x",
            panic_cr2, panic_ins, cs);
        }
        //kprintf("Page fault!\n");
        //kprintf("CR2: 0x%x\n", (unsigned long long int)cr2);
        if( cr2 >= 0xC0000000 ) {
            // map in kernel-global page
            int table_no = (cr2 >> 22); // should always be >= 768
            int table_offset = (cr2 >> 12) & 0x3FF;
            uint32_t *pde = (uint32_t*)(0xFFFFF000 + (table_no*4));
            if( ((*pde) & 1) == 0 ) {
                if( global_kernel_page_directory[table_no-768] != 0 ) {
                    (*pde) = global_kernel_page_directory[table_no-768];
                    
                    uint32_t *table = (uint32_t*)(0xFFC00000+(table_no*0x1000));
                    uint32_t pte = table[table_offset];
                    if( (pte & 1) > 0 ) {
                        // we just loaded the page table holding the faulting entry.
                        // so just return now.
                        in_pagefault = false;
                        panic_ins = 0;
                        panic_cr2 = 0;
                        return;
                    }
                } else {
                    // allocate a new page
                    int frame_id = -1; //pageframe_allocate_single(0);
                    for(int i=0;i<n_blocks[0];i++) {
                        if( !pageframe_get_block_status(i, 0) ) {
                             if( !pageframe_get_block_status(i, 0) ) {
                                int parent = i;
                                for(int j=0;j<=BUDDY_MAX_ORDER;j++) {
                                    pageframe_set_block_status(parent, j, true);
                                    parent >>= 1;
                                }
                                frame_id = i;
                                break;
                            }
                        }
                    }
                    
                    size_t addr = pageframe_get_block_addr(frame_id, 0);
                    (*pde) = addr | 1;
                    global_kernel_page_directory[table_no-768] = addr | 1;
                }
            }
            
            uint32_t *table = (uint32_t*)(0xFFC00000+(table_no*0x1000));
            uint32_t pte = table[table_offset];
            if( (pte & 1) > 0 ) {
                // there's already a mapping present for this page.
                // we need to evict something, but since we don't have mass-storage drivers yet...
                in_pagefault = false;
                panic_ins = 0;
                panic_cr2 = 0;
                return; 
            }
            
            // map in a new page
            int frame_id = -1; //pageframe_allocate_single(0);
            for(int i=0;i<n_blocks[0];i++) {
                if( !pageframe_get_block_status(i, 0) ) {
                     if( !pageframe_get_block_status(i, 0) ) {
                        int parent = i;
                        for(int j=0;j<=BUDDY_MAX_ORDER;j++) {
                            pageframe_set_block_status(parent, j, true);
                            parent >>= 1;
                        }
                        frame_id = i;
                        break;
                    }
                }
            }
            if(frame_id == -1) {
                panic("paging: No pageframes left to allocate!");
            }
            paging_set_pte( (size_t)cr2 & 0xFFFFF000, pageframe_get_block_addr(frame_id, 0), 0x100 ); // load vaddr to newly allocated page (w/ GLOBAL and PRESENT flags)
        } else {
            // map in process-specific page
            int frame_id = -1; //pageframe_allocate_single(0);
            for(int i=0;i<n_blocks[0];i++) {
                if( !pageframe_get_block_status(i, 0) ) {
                     if( !pageframe_get_block_status(i, 0) ) {
                        int parent = i;
                        for(int j=0;j<=BUDDY_MAX_ORDER;j++) {
                            pageframe_set_block_status(parent, j, true);
                            parent >>= 1;
                        }
                        frame_id = i;
                        break;
                    }
                }
            }
            if(frame_id == -1) {
                panic("paging: No pageframes left to allocate!");
            }
            if( !process_current->address_space.map( (size_t)cr2 & 0xFFFFF000, pageframe_get_block_addr(frame_id, 0), 0 ) ) { // load vaddr to newly allocated page
                panic("paging: failed to map in faulting page in process %u!", process_current->id);
            }
        }
    } else {
        // We're dealing with a protection violation.
        if( (error_code & 0x4) == 0 ) {
            // Supervisor mode exception.
            panic("paging: kernel-mode memory protection violation at vaddr 0x%x.\n", (unsigned long long int)cr2);
        } else {
            // User mode exception.
            //kprintf("paging: user-mode memory protection violation at vaddr 0x%x.\n", (unsigned long long int)cr2);
        }
    }
    in_pagefault = false;
    panic_ins = 0;
    panic_cr2 = 0;
}
