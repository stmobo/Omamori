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
    return (bool)(buddy_maps[order][outer_index]&(1<<inner_index));
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
#ifdef DEBUG
    kprintf("%u pageframe ranges detected.\n %u kb available in %u 4kb pages.\n", n_mem_ranges, mem_avail_kb, num_pages);
#endif
    // now allocate space for the buddy maps.
    buddy_maps = (size_t**)kmalloc((BUDDY_MAX_ORDER+1)*sizeof(size_t*));
    for(int i=0;i<=BUDDY_MAX_ORDER;i++) {
        int block_size = 1024*pow(2, i+2);
        int num_blocks = mem_avail_bytes / block_size;
        int num_blk_entries = num_blocks / 8;
#ifdef DEBUG
        kprintf("Allocating space for %u order %u blocks.\n", num_blocks, i);
#endif
        buddy_maps[i] = (size_t*)kmalloc(num_blk_entries);
        n_blocks[i] = num_blocks;
    }
    pageframe_restrict_range( (size_t)buddy_maps, ((size_t)buddy_maps)+((BUDDY_MAX_ORDER+1)*sizeof(size_t*)) );
    // now go back and restrict these ranges of memory from paging
    for(int i=0;i<=BUDDY_MAX_ORDER;i++) {
        int block_size = 1024*pow(2, i+2);
        int num_blocks = mem_avail_bytes / block_size;
        int num_blk_entries = num_blocks / 8;
        pageframe_restrict_range( ((size_t)(buddy_maps[i])), ((size_t)(buddy_maps[i]))+(sizeof(size_t)*num_blk_entries) );
    }
}

void recursive_mark_allocated(int c1_index, int c2_index, signed int order, bool status) {
    if((order < 0) || (order > BUDDY_MAX_ORDER))
        return;
    if( (c1_index < 0) || (c2_index < 0) || (c1_index > n_blocks[order])  || (c2_index > n_blocks[order]))
        return;
    pageframe_set_block_status(c1_index, order, status);
    pageframe_set_block_status(c2_index, order, status);
    recursive_mark_allocated( (c1_index*2), (c1_index*2)+1, order-1, status );
    recursive_mark_allocated( (c2_index*2), (c2_index*2)+1, order-1, status );
}

page_frame* pageframe_allocate_specific(int id, int order) {
    page_frame *frames = NULL;
    if( !pageframe_get_block_status(id, order) ) {
        pageframe_set_block_status(id, order, true);
        recursive_mark_allocated((id<<1), (id<<1)+1, order-1, true);
        int parent = id;
        // ...and then mark its parent and grandparent and ... as allocated.
        for(int j=order;j<=BUDDY_MAX_ORDER;j++) {
            pageframe_set_block_status(parent, order, true);
            parent = ((unsigned int)parent) >> 1;
        }
        frames = (page_frame*)kmalloc(sizeof(page_frame)*(1<<order));
        
#ifdef DEBUG
        kprintf("Memory addresses: 0x%x to 0x%x.\n", get_block_addr((1<<order), 0 ), get_block_addr( (1<<(order+1))-1, 0 ) )
#endif
        
        int k=0;
        for(int j=id*(1<<order);j<((id+1)*(1<<(order)))-1;j++) { // for all j from 2^order to (2^(order+1))-1...
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
    for(int i=0;i<BUDDY_MAX_ORDER;i++) {
        if( (1<<i) == n_frames ) {
            order = i;
        } else if( ( (1<<i) < n_frames) && ((1<<(i+1)) > n_frames)  ) {
            order = i+1;
        }
    }
#ifdef DEBUG
    kprintf("Allocating pageframes.\nAllocating order 0x%x block.\n", order);
#endif
    
    for(int i=0;i<n_blocks[order];i++) {
        if( !pageframe_get_block_status(i, order) ) { // if the block we're not looking at is not allocated...
            return pageframe_allocate_specific(i, order); // then allocate it.
        }
    }
    return NULL;
}

void deallocate_block(int blk_num, int order) {
    pageframe_set_block_status(blk_num, order, false);
    if(!pageframe_get_block_status(pageframe_get_block_buddy(blk_num, order), order)) {
        return deallocate_block( ((unsigned int)blk_num) >> 1, order+1 );
    }
}

void pageframe_deallocate(page_frame* frames, int n_frames) {
    for(int i=0;i<n_frames;i++) {
        deallocate_block(frames[i].id, 0);
    }
}

void pageframe_restrict_range(size_t start_addr, size_t end_addr) {
    int start_block = pageframe_get_block_from_addr(start_addr);
    int end_block = pageframe_get_block_from_addr(end_addr);
    for(int i=start_block;i<=end_block;i++) {
        pageframe_allocate_specific(i, 0);
    }
}