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

unsigned long long int mem_avail_bytes;
int mem_avail_kb;
int num_pages;
memory_map_t* mem_map;
size_t mem_map_len;
int n_mem_ranges;

memory_range* memory_ranges;
size_t **buddy_maps;
int n_blocks[BUDDY_MAX_ORDER+1];

vaddr_range k_vmem_linked_list;
vaddr_range __k_vmem_allocate_start;

bool pageframes_initialized = false;

uint32_t initial_heap_pagetable[1024] __attribute__((aligned(0x1000)));

static mutex __frame_allocator_lock;

uint32_t global_kernel_page_directory[256]; // spans PDE nos. 768 - 1023

size_t pageframe_get_block_addr(int blk_num, int order) {
    int zero_order_blk = blk_num*(1<<order);
    for(int i=0;i<n_mem_ranges;i++) {
        if( (memory_ranges[i].page_index_start <= zero_order_blk) && (zero_order_blk < memory_ranges[i].page_index_end) ) {
            int intrarange_offset = zero_order_blk - memory_ranges[i].page_index_start;
            size_t byte_offset = intrarange_offset*0x1000;
            return memory_ranges[i].base+byte_offset;
        }
    }
    return 0; // invalid page
}

int pageframe_get_block_from_addr(size_t address) {
    size_t fourk_boundary = address - (address%0x1000);
    for(int i=0;i<n_mem_ranges;i++) {
        if( (memory_ranges[i].base <= fourk_boundary) && (fourk_boundary < memory_ranges[i].end) ) {
            size_t byte_offset = fourk_boundary - memory_ranges[i].base;
            int intrarange_offset = byte_offset/0x1000;
            return memory_ranges[i].page_index_start+intrarange_offset;
        }
    }
    return -1;
}

int pageframe_get_block_buddy(int blk_num, int order) {
    return blk_num ^ (1<<order);
}

int pageframe_get_alloc_order( int n_frames ) {
    int order = 0;
    for(int i=0;i<=BUDDY_MAX_ORDER;i++) {
        if( (1<<i) == n_frames ) {
            order = i;
            break;
        } else if( ( (1<<i) < n_frames) && ((1<<(i+1)) > n_frames)  ) {
            order = i+1;
            break;
        }
    }
    return order;
}

// the child blocks of a particular block of order > 0 can be found with
// 2i+1 and 2i -- the buddy maps form binary trees.
// so, for example, order 1 block 4
// has its order 0 children at block ids 8 and 9.

bool pageframe_get_block_status(int blk_num, int order) {
    int zero_order_blk = blk_num*(1<<order);
    if(zero_order_blk > num_pages)
        return false;
    int outer_index = blk_num / 32;
    int inner_index = blk_num % 32;
    return ((buddy_maps[order][outer_index]&(1<<inner_index)) > 0);
}

void pageframe_set_block_status(int blk_num, int order, bool status) {
    int zero_order_blk = blk_num*(1<<order);
    if(zero_order_blk > num_pages)
        return;
    int outer_index = blk_num / 32;
    int inner_index = blk_num % 32;
    if(status)
        buddy_maps[order][outer_index] |= (1<<inner_index);
    else
        buddy_maps[order][outer_index] &= ~(1<<inner_index);
}

void recursive_mark_allocated(int c1_index, int c2_index, signed int order, bool status) {
    if((order < 0) || (order > BUDDY_MAX_ORDER))
        return;
    if( (c1_index < 0) || (c2_index < 0) || (c1_index > n_blocks[order])  || (c2_index > n_blocks[order]))
        return;
    pageframe_set_block_status(c1_index, order, status);
    pageframe_set_block_status(c2_index, order, status);
    recursive_mark_allocated( (c1_index<<1), (c1_index<<1)+1, order-1, status );
    recursive_mark_allocated( (c2_index<<1), (c2_index<<1)+1, order-1, status );
}

page_frame* pageframe_allocate_specific(int id, int order) {
    page_frame *frames = NULL;
    __frame_allocator_lock.lock();
    if( !pageframe_get_block_status(id, order) ) {
        pageframe_set_block_status(id, order, true);
        recursive_mark_allocated((id<<1), (id<<1)+1, order-1, true);
        int parent = id;
        // ...and then mark its parent and grandparent and ... as allocated.
        for(int j=order;j<=BUDDY_MAX_ORDER;j++) {
            pageframe_set_block_status(parent, j, true);
            parent >>= 1;
        }
        frames = (page_frame*)kmalloc(sizeof(page_frame)*(1<<order));
/*        
#ifdef PAGING_DEBUG
        kprintf("Memory addresses: 0x%x to 0x%x.\n", pageframe_get_block_addr((1<<order), 0 ), pageframe_get_block_addr( (1<<(order+1))-1, 0 ) );
#endif
*/
        int k=0;
        for(int j=id*(1<<order);j<((id+1)*(1<<(order)));j++) { // for all j from 2^order to (2^(order+1))-1...
#ifdef PAGING_DEBUG
            //kprintf("Assigning block id %u to frame struct.\n", j);
#endif
            frames[k].id = j;
            frames[k].id_allocated_as = id;
            frames[k].order_allocated_as = order;
            frames[k].address = pageframe_get_block_addr(j, 0);
            k++;
        }
    }
    __frame_allocator_lock.unlock();
    return frames;
}

page_frame* pageframe_allocate(int n_frames) {
    // find out the order of the allocated frame
    int order = 0;
    if( n_frames <= (1<<BUDDY_MAX_ORDER) ) {
        for(int i=0;i<=BUDDY_MAX_ORDER;i++) {
            if( (1<<i) == n_frames ) {
                order = i;
                break;
            } else if( ( (1<<i) < n_frames) && ((1<<(i+1)) > n_frames)  ) {
                order = i+1;
                break;
            }
        }
    } else {
        // find out how many order 8 blocks we'll have to allocate
        // also find out the remainder
        int o8_blocks = n_frames >> BUDDY_MAX_ORDER;
        int remainder = n_frames & ((1<<BUDDY_MAX_ORDER)-1); // % (1 << BUDDY_MAX_ORDER);
        // now find the order of the remaining allocation
        int rem_order = 0;
        for(int i=0;i<=BUDDY_MAX_ORDER;i++) {
            if( (1<<i) == remainder ) {
                rem_order = i;
                break;
            } else if( ( (1<<i) < remainder) && ((1<<(i+1)) > remainder)  ) {
                rem_order = i+1;
                break;
            }
        }
        
        // the number of allocated frames is now ( o8_blocks * (1<<BUDDY_MAX_ORDER) ) + (1<<rem_order)
        int allocated_frames = ( o8_blocks * (1<<BUDDY_MAX_ORDER) ) + (1<<rem_order);
        page_frame* frames = (page_frame*)kmalloc( sizeof(page_frame)*allocated_frames );
        page_frame* f_tmp = NULL;
        
        // now allocate all the blocks
        for(int i=0;i<o8_blocks;i++) {
            f_tmp = pageframe_allocate( (1<<BUDDY_MAX_ORDER) );
            if(f_tmp == NULL) {
                kfree((char*)frames);
                return NULL;
            }
            for( int j=0;j<(1<<BUDDY_MAX_ORDER);j++ ) {
                frames[ (i*(1<<BUDDY_MAX_ORDER))+j ] = f_tmp[j];
            }
            kfree((char*)f_tmp);
        }
        if( remainder > 0) {
            f_tmp = pageframe_allocate( (1<<rem_order) );
            if(f_tmp == NULL) {
                kfree((char*)frames);
                return NULL;
            }
            for(int i=0;i<(1<<rem_order);i++) {
                frames[ (o8_blocks*(1<<BUDDY_MAX_ORDER))+i ] = f_tmp[i];
            }
            kfree((char*)f_tmp);
        }
        return frames;
    }
    
    // Allocate one set of blocks.
    for(int i=0;i<n_blocks[order];i++) {
        __frame_allocator_lock.lock();
        if( !pageframe_get_block_status(i, order) ) { // if the block we're not looking at is not allocated...
            return pageframe_allocate_specific(i, order); // then allocate it.
        }
        __frame_allocator_lock.unlock();
    }
    return NULL;
}

page_frame* pageframe_allocate_at( size_t where, int n_frames ) {
    // find out the order of the allocated frame
    where &= 0xFFFFF000;
    int where_frame = pageframe_get_block_from_addr( where );
    page_frame *frames = (page_frame*)kmalloc(sizeof(page_frame)*n_frames);
    __frame_allocator_lock.lock();
    if( frames != NULL ) {
        for(int i=0;i<n_frames;i++) {
            size_t paddr = where + (i*0x1000);
            frames[i].address = paddr;
            frames[i].id = pageframe_get_block_from_addr( paddr );
            if( frames[i].id != -1) {
                pageframe_allocate_specific( frames[i].id, 0 );
            }
        }
    }
    __frame_allocator_lock.unlock();
    return frames;
}

void pageframe_deallocate_specific(int blk_num, int order) {
    if(order <= BUDDY_MAX_ORDER) {
        __frame_allocator_lock.lock();
        pageframe_set_block_status(blk_num, order, false);
        if(!pageframe_get_block_status(pageframe_get_block_buddy(blk_num, order), order)) {
            __frame_allocator_lock.unlock();
            return pageframe_deallocate_specific( ((unsigned int)blk_num) >> 1, order+1 );
        }
    }
}

void pageframe_deallocate(page_frame* frames, int n_frames) {
    for(int i=0;i<n_frames;i++) {
        pageframe_deallocate_specific(frames[i].id, 0);
    }
    kfree((char*)frames);
}

void pageframe_restrict_range(size_t start_addr, size_t end_addr) {
    int start_block = pageframe_get_block_from_addr(start_addr);
    int end_block = pageframe_get_block_from_addr(end_addr);
    for(int i=start_block;i<=end_block;i++) {
        if( !pageframe_get_block_status(i, 0) ) {
#ifdef PAGING_DEBUG
            kprintf("Pinning page ID %u.\n", i);
#endif
            pageframe_allocate_specific(i, 0);
        }
    }
}

void initialize_vmem_allocator() {
    k_vmem_linked_list.address = 0xC0400000;
    k_vmem_linked_list.free = false;
    k_vmem_linked_list.prev = NULL;
    k_vmem_linked_list.next = &__k_vmem_allocate_start;
    
    __k_vmem_allocate_start.address = 0xC0400000+HEAP_INITIAL_ALLOCATION;
    __k_vmem_allocate_start.free = true;
    __k_vmem_allocate_start.prev = &k_vmem_linked_list;
    __k_vmem_allocate_start.next = NULL;
    
    // first off, map in initial_heap_pagetable.
    // the PDE should already be there.
    uint32_t *table = (uint32_t*)(0xFFC00000+(((size_t)(&initial_heap_pagetable[0])>>22)*0x1000));
    table[ ((size_t)(&initial_heap_pagetable[0])>>12)&0x3FF ] = HEAP_INITIAL_PT_ADDR | 1;
    invalidate_tlb( (size_t)&initial_heap_pagetable[0] );
    
    // map in initial_heap_pagetable as a page table
    (*(uint32_t*)0xFFFFFC04) = (HEAP_INITIAL_PT_ADDR|1);
    
    // now actually map in the initial heap pages
    for(int i=0;i<(HEAP_INITIAL_ALLOCATION/0x1000);i++) {
        initial_heap_pagetable[i] = (HEAP_INITIAL_PHYS_ADDR+(i*0x1000)) | 1;
        invalidate_tlb( 0xC0400000+(i*0x1000) );
        //paging_set_pte( 0xC0400000+(i*0x1000), HEAP_INITIAL_PHYS_ADDR+(i*0x1000), 0 );
    }
    
    for(int i=0;i<256;i++) {
        global_kernel_page_directory[i] = 0;
    }
    global_kernel_page_directory[0] = ((uint32_t)&PageTable768 | 1);
    global_kernel_page_directory[1] = (HEAP_INITIAL_PT_ADDR|1);
}

void initialize_pageframes(multiboot_info_t* mb_info) {
    terminal_writestring("Memory available:\n");
    if(mb_info->flags&(1<<6)) {
        char hex2[16];
        mem_map = (memory_map_t*)kmalloc(mb_info->mmap_length);
        memcpy(mem_map, (void*)mb_info->mmap_addr,mb_info->mmap_length);
        memory_map_t* mmap = mem_map;
        mem_map_len = mb_info->mmap_length;
        int n_pageframes = 0;
        
        while((size_t)mmap < (size_t)mem_map+mem_map_len) {
            if (mmap->type == 1) {
                n_mem_ranges++;
            }
            mmap = (memory_map_t*)( (unsigned int)mmap + mmap->size + sizeof(unsigned int) );
        }
        
        memory_ranges = (memory_range*)kmalloc(n_mem_ranges*sizeof(memory_range));
        mmap = mem_map;
        
        int i = 0;
        while((size_t)mmap < (size_t)mem_map+mem_map_len) {
            unsigned long long int base = (mmap->base_addr_high<<16) + mmap->base_addr_low;
            unsigned long long int length = (mmap->length_high<<16) + mmap->length_low;
            if (mmap->type == 1) {
                unsigned long long int end = base + length;
                mem_avail_bytes += length;
                memory_ranges[i].base = base;
                memory_ranges[i].length = length;
                memory_ranges[i].end = end;
                memory_ranges[i].page_index_start = n_pageframes;
                memory_ranges[i].page_index_end = n_pageframes + (length / 4096);
                memory_ranges[i].n_pageframes = length / 4096;
                n_pageframes += (length / 4096);
                i++;
                kprintf("Avail: 0x%x - 0x%x (%u bytes)\n", base, end, length);
            }
            mmap = (memory_map_t*)( (unsigned int)mmap + mmap->size + sizeof(unsigned int) );
        }
    }
    mem_avail_kb = mem_avail_bytes / 1024;
    num_pages = mem_avail_kb / 4;
#ifdef PAGING_DEBUG
    kprintf("%u pageframe ranges detected.\n %u kb available in %u 4kb pages.\n", n_mem_ranges, mem_avail_kb, num_pages);
#endif
    // now allocate space for the buddy maps.
    buddy_maps = (size_t**)kmalloc((BUDDY_MAX_ORDER+1)*sizeof(size_t*));
    for(int i=0;i<=BUDDY_MAX_ORDER;i++) {
        int block_size = 1024*pow(2, i+2);
        int num_blocks = ((uint32_t)mem_avail_bytes) / block_size;
        // since this is a 32-bit system, we can assume that mem_avail_bytes can be casted to a uint32_t without
        // a loss in precision.
        int num_blk_entries = num_blocks / 8;
#ifdef PAGING_DEBUG
        kprintf("Allocating space for %u order %u blocks.\n", num_blocks, i);
#endif
        buddy_maps[i] = (size_t*)kmalloc(num_blk_entries);
        n_blocks[i] = num_blocks;
    }
    
    //pageframe_restrict_range( (size_t)&kernel_start_phys, (size_t)&kernel_end_phys );
    pageframe_restrict_range( HEAP_INITIAL_PT_ADDR, HEAP_INITIAL_PT_ADDR+0xFFF );
    pageframe_restrict_range( 0, 0x400000 );
    pageframe_restrict_range( HEAP_INITIAL_PHYS_ADDR, HEAP_INITIAL_PHYS_ADDR+HEAP_INITIAL_ALLOCATION );
    //k_vmem_alloc( 0xC0000000, 0xC0400000 );
    k_vmem_alloc( (size_t)buddy_maps, (size_t)(((size_t)buddy_maps)+((BUDDY_MAX_ORDER+1)*sizeof(size_t*))) );
    kprintf("Map of buddy maps begins at 0x%x and ends at 0x%x\n", (unsigned long long int)buddy_maps, (unsigned long long int)(((size_t)buddy_maps)+((BUDDY_MAX_ORDER+1)*sizeof(size_t*))) );
    // now go back and restrict these ranges of memory from paging
    for(int i=0;i<=BUDDY_MAX_ORDER;i++) {
        int block_size = 1024*pow(2, i+2);
        int num_blocks = ((uint32_t)mem_avail_bytes) / block_size;
        // since this is a 32-bit system, we can assume that mem_avail_bytes can be casted to a uint32_t without
        // a loss in precision.
        int num_blk_entries = num_blocks / 8;
        k_vmem_alloc( (size_t)(buddy_maps[i]), (size_t)(((size_t)(buddy_maps[i]))+(sizeof(size_t)*num_blk_entries)) );
#ifdef PAGING_DEBUG
        kprintf("Buddy map for order %u begins at 0x%x and ends at 0x%x\n", (unsigned long long int)i, ((unsigned long long int)(buddy_maps[i])), (unsigned long long int)(((size_t)(buddy_maps[i]))+(sizeof(size_t)*num_blk_entries)) );
#endif
    }
    pageframes_initialized = true;
}

inline void invalidate_tlb(size_t address) {
    asm volatile("invlpg (%0)" : : "r"(address) : "memory");
}

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
        // so right now we just exit noisily.
        //kprintf("paging: Attempted to map vaddr 0x%x when mapping already present!\n", (unsigned long long int)vaddr);
        return; 
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
    if( (pte & 1) > 0 ) {
        // no PTE for address
        return 0xFFFFFFFF;
    }
    return table[table_offset];
}

// Allocate <n_pages> pages of virtual memory from a predefined list
size_t paging_vmem_alloc( vaddr_range *start, size_t maximum_address, int n_pages) {
    vaddr_range *current = start;
    size_t n_bytes = n_pages * 0x1000;
    while( current != NULL ) {
        if( current->free ) {
            unsigned int len = maximum_address-1;
            if( current->next != NULL ) {
                len = current->next->address - 1;
            }
            len -= current->address;
            if( len == n_bytes ) {
                current->free = false;
                return current->address & 0xFFFFF000;
            } else if( len > n_bytes ) {
                current->free = false;
                vaddr_range *next = current->next;
                current->next = new vaddr_range;
                current->next->free = true;
                current->next->address = (current->address)+n_bytes;
                 
                current->next->prev = current;
                current->next->next = next;
                if(next != NULL) { 
                    next->prev = current->next;
                }
                return current->address & 0xFFFFF000;
            }
        }
        current = current->next;
    }
    return NULL;
}

// Allocate a specific memory range.
size_t paging_vmem_alloc_specific( vaddr_range *start, size_t start_addr, size_t end_addr) {
    vaddr_range *current = start;
    size_t n_bytes = end_addr - start_addr;
    while( current != NULL ) {
        if( (current->address <= start_addr) && (current->free) && ((current->next == NULL) || (current->next->address >= end_addr)) ) {
            if( current->address == start_addr ) {
                current->free = false;
            } else if( current->address < start_addr ) {
                vaddr_range *next = current->next;
                current->next = new vaddr_range;
                current->next->free = true;
                current->next->address = (start_addr & 0xFFFFF000);
                 
                current->next->prev = current;
                current->next->next = next;
                next->prev = current->next;
                current = current->next;
            }
            vaddr_range *next = current->next;
            current->next = new vaddr_range;
            current->next->free = true;
            current->next->address = (end_addr & 0xFFFFF000);
             
            current->next->prev = current;
            current->next->next = next;
            next->prev = current->next;
            return current->next->address;
        }
        current = current->next;
    }
    return NULL;
}

bool paging_vmem_free( vaddr_range *start, size_t address ) {
    vaddr_range *current = start;
    while( current != NULL ) {
        if( current->address == address ) {
            current->free = true;
            if( ( current->prev != NULL ) && current->prev->free ) {
                current->prev->next = current->next;
                if( current->next ) {
                    current->next->prev = current->prev;
                }
                
                /*
                current->next = NULL;
                current->prev = NULL;
                current->address = NULL;
                current->free = false;
                kprintf("paging_vmem_free: deleting current node in list.\n");
                */
                
                delete current;
            } else if( ( current->next != NULL ) && current->next->free ) {
                vaddr_range *next = current->next;
                if(next) { // dunno why we need to do this
                    if( next->next != NULL ) {
                        next->next->prev = current;
                    }
                    current->next = next->next;
                    
                    /*
                    current->next->next = NULL;
                    current->next->prev = NULL;
                    current->next->address = NULL;
                    current->next->free = false;
                    kprintf("paging_vmem_free: deleting next node in list.\n");
                    */
                    
                    delete next;
                }
            }
            return true;
        }
        current = current->next;
    }
    return false;
}

size_t k_vmem_alloc( int n_pages ) {
    return paging_vmem_alloc( &k_vmem_linked_list, (size_t)0xFFC00000, n_pages );
}

size_t k_vmem_alloc( size_t begin, size_t end ) {
    return paging_vmem_alloc_specific( &k_vmem_linked_list, begin, end );
}

size_t k_vmem_free( size_t address ) {
    return paging_vmem_free( &k_vmem_linked_list, address );
}

size_t paging_map_phys_address( size_t paddr, int n_frames ) {
    page_frame *frames = pageframe_allocate_at( paddr, n_frames );
    size_t vaddr = k_vmem_alloc( n_frames );
    if( vaddr == NULL ) {
        kprintf("paging_map_phys_address: could not find free vaddr!\n");
        return NULL;
    }
    if( frames != NULL ) {
        for( int i=0;i<n_frames;i++ ) {
            paging_set_pte( vaddr+(i*0x1000), frames[i].address, 0 );
        }
        return vaddr;
    } else {
        kprintf("paging_map_phys_address: could not allocate space for page_frame*!\n");
        k_vmem_free( vaddr );
    }
    return NULL;
}

void paging_unmap_phys_address( size_t vaddr, int n_frames ) {
    size_t base_paddr = paging_get_pte( vaddr&0xFFFFF000 ) & 0xFFFFF000;
    int base_id = pageframe_get_block_from_addr( base_paddr );
    page_frame *frame = (page_frame*)kmalloc(sizeof(page_frame));
    
    for(int i=0;i<n_frames;i++) {
        size_t paddr = paging_get_pte( vaddr+(i*0x1000) ) & 0xFFFFF000;
        int id = pageframe_get_block_from_addr( paddr );
        if( id != -1 ){
            frame->address = paddr;
            frame->id = id;
            pageframe_deallocate(frame, 1);
        }
        paging_unset_pte( vaddr+(i*0x1000) );
    }
    
    k_vmem_free( vaddr );
}

void copy_pageframe_range( uint32_t src_page, uint32_t dst_page, int n_pages ) {
    src_page &= 0xFFFFF000;
    dst_page &= 0xFFFFF000;
    
    size_t tmp_pages = k_vmem_alloc(2);
    
    uint32_t* src_v_page = (uint32_t*)tmp_pages;
    uint32_t* dst_v_page = (uint32_t*)(tmp_pages+0x1000);
    
    for(int i=0;i<n_pages;i++) {
        paging_set_pte( tmp_pages, src_page+(i*0x1000), 0 );
        paging_set_pte( tmp_pages+0x1000, dst_page+(i*0x1000), 0 );
        
        for(int j=0;j<0x1000;j++) {
            dst_v_page[j] = src_v_page[j];
        }
    }
    
    paging_unset_pte( tmp_pages );
    paging_unset_pte( tmp_pages+0x1000 );
    k_vmem_free(tmp_pages);
}

page_frame* duplicate_pageframe_range( uint32_t src_page, int n_pages ) {
    src_page &= 0xFFFFF000;
    
    size_t tmp_pages = k_vmem_alloc(2);
    page_frame *new_frames = pageframe_allocate(n_pages);
    
    uint32_t* src_v_page = (uint32_t*)tmp_pages;
    uint32_t* dst_v_page = (uint32_t*)(tmp_pages+0x1000);
    
    for(int i=0;i<n_pages;i++) {
        paging_set_pte( tmp_pages, src_page+(i*0x1000), 0 );
        paging_set_pte( tmp_pages+0x1000, new_frames[i].address, 0 );
        
        for(int j=0;j<0x1000;j++) {
            dst_v_page[j] = src_v_page[j];
        }
    }
    
    paging_unset_pte( tmp_pages );
    paging_unset_pte( tmp_pages+0x1000 );
    k_vmem_free(tmp_pages);
    return new_frames;
}

size_t mmap(int n_pages) {
    size_t alloc_start = k_vmem_alloc(n_pages);
    page_frame *alloc_frames = pageframe_allocate(n_pages);
    if( alloc_start != NULL && alloc_frames != NULL ) {
        for(int i=0;i<n_pages;i++) {
            paging_set_pte( alloc_start+(i*0x1000), alloc_frames[i].address, 0 );
        }
    }
    return alloc_start;
}

void munmap(size_t alloc_start, int n_pages) {
    for(int i=0;i<n_pages;i++) {
        size_t vaddr = alloc_start + (i*0x1000);
        size_t paddr = paging_get_pte( vaddr ) & 0xFFFFF000;
        pageframe_deallocate_specific( pageframe_get_block_from_addr( paddr ), 0 );
        paging_unset_pte( vaddr );
    }
    k_vmem_free(alloc_start);
}

// hopefully this is reentrant. Hopefully.
int pageframe_allocate_single(int order) {
    int frame_id = -1;
    for(int i=0;i<n_blocks[order];i++) {
        if( !pageframe_get_block_status(i, order) ) {
             if( !pageframe_get_block_status(i, order) ) {
                if(order != 0)
                    recursive_mark_allocated((i<<1), (i<<1)+1, order-1, true);
                int parent = i;
                for(int j=order;j<=BUDDY_MAX_ORDER;j++) {
                    pageframe_set_block_status(parent, j, true);
                    parent >>= 1;
                }
                frame_id = i;
                break;
            }
        }
    }
    return frame_id;
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
        panic("paging: page fault in page fault handler!\npaging: initial CR2: 0x%x\npaging: recursive CR2: 0x%x", panic_cr2, recursive_cr2);
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
            process_current->address_space.map( (size_t)cr2 & 0xFFFFF000, pageframe_get_block_addr(frame_id, 0), 0x06 ); // load vaddr to newly allocated page
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