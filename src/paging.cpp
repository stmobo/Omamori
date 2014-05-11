// page frame size is 4 kilobytes
// we're using a buddy allocator system for page frames.

// Our test system has 2GB, in 2096700 page frames.
// So, our order 0 bitmap is 256 KB.
// we have bitmaps for everything up to order 10 (1MB blocks).
// In total, our bitmaps will take up 512KB of space.

#include "includes.h"
#include "multiboot.h"
#include "paging.h"
#include "vga.h"
#include "vector.h"

#include "ps2_keyboard.h"

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

uint32_t pagedir[1024];

unsigned long long int mem_avail_bytes;
int mem_avail_kb;
int num_pages;
memory_map_t* mem_map;
size_t mem_map_len;
int n_mem_ranges;

memory_range* memory_ranges;
size_t **buddy_maps;
int n_blocks[BUDDY_MAX_ORDER+1];

uint32_t *page_directory;
// instead of having a 1024-large array of pointers to the page tables,
// we'll instead use the page tables themselves to determine what pages they're located in.

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
        
        kprintf("Allocating %u order %u blocks and %u remainder frames.\n", (unsigned long long int)o8_blocks, (unsigned long long int)BUDDY_MAX_ORDER, (unsigned long long int)remainder );
        
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
        kprintf("Remainder block order is %u.\n", rem_order);
        
        // the number of allocated frames is now ( o8_blocks * (1<<BUDDY_MAX_ORDER) ) + (1<<rem_order)
        int allocated_frames = ( o8_blocks * (1<<BUDDY_MAX_ORDER) ) + (1<<rem_order);
        page_frame* frames = (page_frame*)kmalloc( sizeof(page_frame)*allocated_frames );
        page_frame* f_tmp = NULL;
        
        // now allocate all the blocks
        for(int i=0;i<o8_blocks;i++) {
            f_tmp = pageframe_allocate( (1<<BUDDY_MAX_ORDER) );
            for( int j=0;j<(1<<BUDDY_MAX_ORDER);j++ ) {
                frames[ (i*(1<<BUDDY_MAX_ORDER))+j ] = f_tmp[j];
            }
        }
        f_tmp = pageframe_allocate( (1<<rem_order) );
        for(int i=0;i<(1<<rem_order);i++) {
            frames[ (o8_blocks*(1<<BUDDY_MAX_ORDER))+i ] = f_tmp[i];
        }
        return frames;
    }
#ifdef PAGING_DEBUG
    kprintf("Allocating order %u block.\n", ((unsigned long long int)order));
#endif
    
    for(int i=0;i<n_blocks[order];i++) {
#ifdef PAGING_DEBUG
        kprintf("Inspecting block ID: %u, begins at ID %u.\n", ((unsigned long long int)i), ((unsigned long long int)i*(1<<order)));
#endif
        if( !pageframe_get_block_status(i, order) ) { // if the block we're not looking at is not allocated...
//#ifdef PAGING_DEBUG
            kprintf("Allocating block ID: %u, order %u\n", ((unsigned long long int)i), ((unsigned long long int)order));
            kprintf("Block starts at ID %u.\n", ((unsigned long long int)i*(1<<order)));
//#endif
            return pageframe_allocate_specific(i, order); // then allocate it.
        }
    }
    return NULL;
}

void pageframe_deallocate_specific(int blk_num, int order) {
    if(order <= BUDDY_MAX_ORDER) {
        pageframe_set_block_status(blk_num, order, false);
        if(!pageframe_get_block_status(pageframe_get_block_buddy(blk_num, order), order)) {
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

void initialize_pageframes(multiboot_info_t* mb_info) {
    terminal_writestring("Memory available:\n");
    if(mb_info->flags&(1<<6)) {
        char hex2[16];
        mem_map = (memory_map_t*)kmalloc(mb_info->mmap_length);
        memcpy((char*)mem_map, (char*)(mb_info->mmap_addr),mb_info->mmap_length);
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
        int num_blocks = mem_avail_bytes / block_size;
        int num_blk_entries = num_blocks / 8;
#ifdef PAGING_DEBUG
        kprintf("Allocating space for %u order %u blocks.\n", num_blocks, i);
#endif
        buddy_maps[i] = (size_t*)kmalloc(num_blk_entries);
        n_blocks[i] = num_blocks;
    }
    
}

void paging_set_pte(size_t vaddr, size_t paddr, uint16_t flags) {
    int table_no = (vaddr >> 22);
    int table_offset = (vaddr >> 12) & 0x3FF;
    
    uint32_t *table = (uint32_t*)(0xFFC00000+(table_no*0x1000));
    
    table[table_offset] = (paddr | (flags&0x7FF) | 0x01);
}

uint32_t paging_get_pte(size_t vaddr) {
    int table_no = (vaddr >> 22);
    int table_offset = (vaddr >> 12) & 0x3FF;
    
    uint32_t *table = (uint32_t*)(0xFFC00000+(table_no*0x1000));
    return table[table_offset];
}

void initialize_paging() {
    pageframe_restrict_range( 0x100000, (size_t)&kernel_end );
    pageframe_restrict_range( (size_t)buddy_maps, ((size_t)buddy_maps)+((BUDDY_MAX_ORDER+1)*sizeof(size_t*)) );
    kprintf("Map of buddy maps begins at 0x%x and ends at 0x%x\n", (unsigned long long int)buddy_maps, (unsigned long long int)(((size_t)buddy_maps)+((BUDDY_MAX_ORDER+1)*sizeof(size_t*))) );
    // now go back and restrict these ranges of memory from paging
    for(int i=0;i<=BUDDY_MAX_ORDER;i++) {
        int block_size = 1024*pow(2, i+2);
        int num_blocks = mem_avail_bytes / block_size;
        int num_blk_entries = num_blocks / 8;
        pageframe_restrict_range( ((size_t)(buddy_maps[i])), ((size_t)(buddy_maps[i]))+(sizeof(size_t)*num_blk_entries) );
        kprintf("Buddy map for order %u begins at 0x%x and ends at 0x%x\n", (unsigned long long int)i, ((unsigned long long int)(buddy_maps[i])), (unsigned long long int)(((size_t)(buddy_maps[i]))+(sizeof(size_t)*num_blk_entries)) );
    }
    pageframe_restrict_range(0, 0x500);
    
    kprintf("Allocating frames for tables.\n");
    page_frame *frames = pageframe_allocate(1024); // 1023 frames for the tables and 1 for the directory
    page_directory = (uint32_t*)frames[0].address;
    /*
    for(int i=0;i<1024;i++) {
        kprintf("frames[%u].address=0x%x\n", (unsigned long long int)i, (unsigned long long int)frames[i].address);
        ps2_keyboard_get_keystroke();
    } */
    kprintf("Loading page directory at address 0x%x\n", (unsigned long long int)frames[0].address);
    for(int i=0;i<1023;i++) {
        page_directory[i] = frames[i+1].address | 3; // set read-write permissions bit
    }
    page_directory[1023] = (uint32_t)page_directory | 3; // map last PDE to the page directory itself
    // Page tables start at 0xFFC00000 (0xFFFFFFFF-0x40000000+1), and the directory itself is the very last page, 0xFFFFF000.
    
    // now set up mappings
    
    kprintf("Setting up initial page mappings.\n");
    
    // ID-map the 1st MB
    uint32_t *pt0 = (uint32_t*)frames[1].address;
    kprintf("Identity mapping 1st MB, pt0 at address 0x%x\n", (unsigned long long int)frames[1].address);
    for(size_t i=0;i<0x100000;i+=0x1000) {
        pt0[ i >> 0x0C ] = i | 3;
    }
    
    // now, map 0x100000 - &kernel_end to PAGING_KERNEL_BASE_ADDR.
    int pt_no = (PAGING_KERNEL_BASE_ADDR >> 22);
    uint32_t *ptn = (uint32_t*)frames[pt_no].address;
    kprintf("Remapping kernel, target table (no. %u) at address 0x%x\n", (unsigned long long int)pt_no, (unsigned long long int)ptn);
    if(ptn == NULL) {
        kprintf("Something is very, very wrong. ptn==NULL!\n");
        asm volatile("int $3\n\t" : : : "memory");
        while(true) {
            asm volatile("cli\n\t"
            "hlt\n\t" : : : "memory");
        }
    }
    for(size_t i=0x100000;i<=((size_t)&kernel_end);i+=0x1000) {
        ptn[ (i-0x100000) ] = i | 5;
        //paging_set_pte( PAGING_KERNEL_BASE_ADDR+(i-0x100000), i, 5 ); // Access set to all, but write protect enabled (and present bit, ofc)
    }
    
    // Now activate paging and hope we don't crash.
    kprintf("Now activating paging.\n");
    
    asm volatile("mov %0, %%eax\n\t"
                 "mov %%eax, %%cr3\n\t"
                 "mov %%cr0, %%eax\n\t"
                 "or 0x80000000, %%eax\n\t"
                 "mov %%eax, %%cr0\n\t"
                 : : "r"(page_directory) : "%eax", "memory");
}