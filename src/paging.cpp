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

size_t get_block_addr(int blk_num, int order) {
    char hex[9];
    hex[8] = '\0';
    int zero_order_blk = blk_num*(1<<order);
    /*
    int_to_hex(zero_order_blk, hex);
    terminal_writestring("Finding address of block, id=");
    terminal_writestring(hex);
    terminal_writestring(".\n");
    */
    int range_n = 0;
    for(int i=0;i<n_mem_ranges;i++) {
        /*
        terminal_writestring("Examining memory range ");
        terminal_writestring(int_to_decimal(i));
        terminal_writestring(", ranging from IDs");
        terminal_writestring(int_to_decimal(memory_ranges[i].page_index_start));
        terminal_writestring(" to ");
        terminal_writestring(int_to_decimal(memory_ranges[i].page_index_end));
        terminal_writestring(".\n");
        */
        if( (memory_ranges[i].page_index_start <= zero_order_blk) && (zero_order_blk < memory_ranges[i].page_index_end) ) {
            int intrarange_offset = zero_order_blk - memory_ranges[i].page_index_start;
            int byte_offset = intrarange_offset*4096;
            return memory_ranges[i].base+byte_offset;
        }
    }
    return 0; // invalid page
}

int get_block_buddy(int blk_num, int order) {
    return blk_num ^ (1<<order);
}

// the child blocks of a particular block of order > 0 can be found with
// 2i+1 and 2i -- the buddy maps form binary trees.
// so, for example, order 1 block 4
// has its order 0 children at block ids 8 and 9.

bool get_block_status(int blk_num, int order) {
    int zero_order_blk = blk_num*(1<<order);
    if(zero_order_blk > num_pages)
        return false;
    int outer_index = blk_num / 32;
    int inner_index = blk_num % 32;
    return (bool)(buddy_maps[order][outer_index]&(1<<inner_index));
}

void set_block_status(int blk_num, int order, bool status) {
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
    char hex[16];
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
                terminal_writestring("Avail: 0x");
                ll_to_hex(base, hex);
                ll_to_hex(end, hex2);
                terminal_writestring(hex, 16);
                terminal_writestring(" - 0x");
                terminal_writestring(hex2, 16); 
                terminal_writestring(" (0x");
                ll_to_hex(length, hex2);
                terminal_writestring(hex2, 16);
                terminal_writestring(" bytes)\n");
                mem_avail_bytes += length;
                memory_ranges[i].base = base;
                memory_ranges[i].length = length;
                memory_ranges[i].end = end;
                memory_ranges[i].page_index_start = n_pageframes;
                memory_ranges[i].page_index_end = n_pageframes + (length / 4096);
                /*
                terminal_writestring("Page index ranges: ");
                terminal_writestring(int_to_decimal(memory_ranges[i].page_index_start));
                terminal_writestring(" to ");
                terminal_writestring(int_to_decimal(memory_ranges[i].page_index_end));
                terminal_writestring(".\n");
                */
                memory_ranges[i].n_pageframes = length / 4096;
                n_pageframes += (length / 4096)+1;
                i++;
            }
            mmap = (memory_map_t*)( (unsigned int)mmap + mmap->size + sizeof(unsigned int) );
        }
    }
    mem_avail_kb = mem_avail_bytes / 1024;
    num_pages = mem_avail_kb / 4;
#ifdef DEBUG
    terminal_writestring(int_to_decimal(n_mem_ranges));
    terminal_writestring(" pageframe ranges detected.\n");
    int_to_hex(mem_avail_kb, hex);
    terminal_writestring("0x");
    terminal_writestring(hex, 8);
    terminal_writestring(" kb available in 0x");
    int_to_hex(num_pages, hex);
    terminal_writestring(hex, 8);
    terminal_writestring(" 4kb pages.\n");
#endif
    // now allocate space for the buddy maps.
    buddy_maps = (size_t**)kmalloc((BUDDY_MAX_ORDER+1)*sizeof(size_t*));
    for(int i=0;i<=BUDDY_MAX_ORDER;i++) {
        int block_size = 1024*pow(2, i+2);
        int num_blocks = mem_avail_bytes / block_size;
        int num_blk_entries = num_blocks / 8;
#ifdef DEBUG
        terminal_writestring("Allocating space for ");
        terminal_writestring(int_to_decimal(num_blocks));
        terminal_writestring(" order ");
        terminal_writestring(int_to_decimal(i));
        terminal_writestring(" blocks.\n");
#endif
        buddy_maps[i] = (size_t*)kmalloc(num_blk_entries);
        n_blocks[i] = num_blocks;
    }
}

void recursive_mark_allocated(int c1_index, int c2_index, signed int order, bool status) {
    if((order < 0) || (order > BUDDY_MAX_ORDER))
        return;
    if( (c1_index < 0) || (c2_index < 0) || (c1_index > n_blocks[order])  || (c2_index > n_blocks[order]))
        return;
    set_block_status(c1_index, order, status);
    set_block_status(c2_index, order, status);
    recursive_mark_allocated( (c1_index*2), (c1_index*2)+1, order-1, status );
    recursive_mark_allocated( (c2_index*2), (c2_index*2)+1, order-1, status );
}

page_frame* pageframe_allocate(int n_frames) {
    // find out the order of the allocated frame
    int order = 0;
#ifdef DEBUG
    char hex[9];
    hex[8] = '\0';
#endif
    if(n_frames > 1) {
        for(int i=0;i<BUDDY_MAX_ORDER;i++) {
            if( ( (1<<i) <= n_frames) && ((1<<(i+1)) > n_frames)  ) {
                order = i+1;
            }
        }
    }
#ifdef DEBUG
    terminal_writestring("Allocating pageframes. \nAllocating order 0x");
    int_to_hex(order, hex);
    terminal_writestring(hex);
    terminal_writestring(" block.");
#endif
    // now allocate a block.
    
    // (i = <order>)
    // Look at all the blocks of order i.
    // Allocate a new block (of id <id>) and mark its parents and children as allocated.
    
    page_frame *frames = NULL; // returned frame pointers
    for(int i=0;i<n_blocks[order];i++) {
        if( !get_block_status(i, order) ) { // if the block we're not looking at is not allocated...
            /*
            terminal_writestring("Found block! ID=0x");
            int_to_hex(i, hex);
            terminal_writestring(hex);
            terminal_writestring("\n");
            */
            // ...allocate it and mark its children as allocated.
            set_block_status(i, order, true);
            recursive_mark_allocated((i<<1), (i<<1)+1, order-1, true);
            int parent = i;
            // ...and then mark its parent and grandparent and ... as allocated.
            for(int j=order;j<=BUDDY_MAX_ORDER;j++) {
                set_block_status(parent, order, true);
                parent = ((unsigned int)parent) >> 1;
            }
            frames = (page_frame*)kmalloc(sizeof(page_frame)*(1<<order));
            // ...now get the addresses of the pageframes.
            /*
            terminal_writestring("Allocated pages - IDs from 0x");
            int_to_hex(i*(1<<order), hex);
            terminal_writestring(hex);
            terminal_writestring(" to 0x");
            int_to_hex(((i+1)*(1<<(order)))-1, hex);
            terminal_writestring(hex);
            terminal_writestring(".\n");
            */
#ifdef DEBUG
            terminal_writestring("Memory addresses: 0x");
            int_to_hex( get_block_addr((1<<order), 0 ), hex );
            terminal_writestring(hex);
            terminal_writestring(" to 0x");
            int_to_hex( get_block_addr( (1<<(order+1))-1, 0 ), hex );
            terminal_writestring(hex);
            terminal_writestring(".\n");
#endif
            
            for(int j=i*(1<<order);j<((i+1)*(1<<(order)))-1;j++) { // for all j from 2^order to (2^(order+1))-1...
                frames[j].id = j;
                frames[j].id_allocated_as = i;
                frames[j].order_allocated_as = order;
                frames[j].address = get_block_addr(j, 0);
            }
            break;
        }
    }
    return frames;
}

void deallocate_block(int blk_num, int order) {
    set_block_status(blk_num, order, false);
    if(!get_block_status(get_block_buddy(blk_num, order), order)) {
        return deallocate_block( ((unsigned int)blk_num) >> 1, order+1 );
    }
}

void pageframe_deallocate(page_frame* frames, int n_frames) {
    for(int i=0;i<n_frames;i++) {
        deallocate_block(frames[i].id, 0);
    }
}