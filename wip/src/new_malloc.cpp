// new_malloc.cpp
// A newer malloc for a newer kernel.

#include "includes.h"
#include "core/paging.h"

#define PAGE_SIZE                       0x1000
#define HEAP_MEMBLOCK_SIZE              32
#define HEAP_PAGE_SET_SIZE              128
// the order of 1 page set in the buddy allocator
// this is equal to log2(HEAP_PAGE_SET_SIZE)
#define HEAP_PAGE_SET_ORDER             7
#define HEAP_SET_SIZE                   (PAGE_SIZE*HEAP_PAGE_SET_SIZE)
#define HEAP_MAX_SETS                   1024
#define HEAP_SET0_START                 0xC0400000
#define HEAP_INITIAL_SETS_LENGTH        0x400000
// 1024 * 128 = 524288KB, or half of kernel space.
#define HEAP_HEADER_STATUS_FREE         0xDEA110C8
#define HEAP_HEADER_STATUS_USED         0xA110C8ED
#define KMALLOC_RESTART_COUNT                    5

#define KMALLOC_NO_RESTART              0x00000001
#define KMALLOC_RESTART_ONCE            0x00000002
#define KMALLOC_RESTART_MANY            (KMALLOC_RESTART_COUNT<<24) | 3
// High byte determines the amount of restarting

k_heap_header *heap_start;

size_t allocator_sets[HEAP_MAX_SETS];

struct k_heap_header = {
    uint32_t status = HEAP_HEADER_STATUS_USED;
    k_heap_header *next = NULL;
}

void k_heap_init() {
    // we already have a few allocator sets set aside for us by paging.cpp.
    int n_initial_sets = HEAP_INITIAL_SETS_LENGTH  / HEAP_SET_SIZE;
    for(int i=0;i<HEAP_MAX_SETS;i++) {
        allocator_sets[i] = -1;
    }
    
    for(int i=0;i<n_initial_sets;i++) {
        allocator_sets[i] = (HEAP_SET0_START+(i*HEAP_SET_SIZE));
    }
    heap_start = (k_heap_header*)(allocator_sets[0]);
    heap_start->status = HEAP_HEADER_STATUS_FREE;
}

void *kmalloc(size_t length, unsigned int flags) {
    k_heap_header *current = heap_start;
    int current_set = 0;
    int next_set = 0;
    size_t init_length = length;
    if(length < HEAP_MEMBLOCK_SIZE)
        length = HEAP_MEMBLOCK_SIZE;
    // find the nearest multiple of the block size
    length = ((length - (length % HEAP_MEMBLOCK_SIZE)) + HEAP_MEMBLOCK_SIZE);
    if( length > HEAP_PAGE_SET_SIZE*PAGE_SIZE ) {
        // allocate entire pages
        int n_pages = ((length - (length % 0x1000))/0x1000)+1;
        return (void*)k_vmem_alloc(n_pages);
    }
    while( current != NULL ) {
        if( __sync_bool_compare_and_swap( current->status, HEAP_HEADER_STATUS_FREE, HEAP_HEADER_STATUS_USED ) ) {
            // using an atomic CAS both locks the block and saves us the trouble of marking it as "used" if we /do/ use it
            if( current->next != NULL ) {
                size_t block_len = (current->next - (current+sizeof(k_heap_header)));
                for(int i=0;i<HEAP_MAX_SETS;i++) {
                    if( (allocator_sets[i] <= (size_t)current->next) && ( (allocator_sets[i]+HEAP_SET_SIZE) < (size_t)current->next ) ) {
                        next_set = i;
                        break;
                    }
                }
                if( current_set != next_set ) {
                    // this block and the next are on different sets
                    // so we need to instead find the length to the end of the set
                    size_t end_of_set = (allocator_sets[current_set]+HEAP_SET_SIZE)-1;
                    block_len = (end_of_set - (size_t)current);
                }
                if( block_len < length ) { // can't use this block
                    current->status = HEAP_HEADER_STATUS_FREE;
                    // fall through
                } else if( block_len >= length ) {
                    if( (block_len - length) >= sizeof(k_heap_header)+HEAP_MEMBLOCK_SIZE ) { // can we put another block down?
                        // okay, so we can
                        // add another block
                        k_heap_header *new_block = ((size_t)current+(length+sizeof(k_heap_header))+1);
                        new_block->next = current->next;
                        new_block->status = HEAP_HEADER_STATUS_FREE;
                        current->next = new_block; // the new block isn't reachable until we do this
                    }
                    // otherwise, we can't, so we don't add a new block
                    // (remember: current->status already == HEAP_HEADER_STATUS_USED)
                    return (void*)((size_t)current+sizeof(k_heap_header)+1);
                }
            } else {
                // okay so we're at the end of the list, and we need to add a new block
                // where that block is is a matter of how far we are into the page
                size_t end_of_set = (allocator_sets[current_set]+HEAP_SET_SIZE)-1;
                size_t avail_len = (end_of_set - (size_t)current);
                if( (avail_len > length) && ((avail_len - length) >= sizeof(k_heap_header)+HEAP_MEMBLOCK_SIZE) ) { // can we just put another block down?
                    k_heap_header *new_block = ((size_t)current+(length+sizeof(k_heap_header))+1);
                    new_block->next = NULL;
                    new_block->status = HEAP_HEADER_STATUS_FREE;
                    current->next = new_block;
                    return (void*)((size_t)current+sizeof(k_heap_header)+1);
                } else {
                    // not enough space left on the current set
                    // go to the next set
                    current_set++;
                    if(allocator_sets[current_set] == -1) {
                        // the next set hasn't been allocated yet,
                        // so allocate it.
                        size_t new_set = k_vmem_alloc(HEAP_PAGE_SET_SIZE);
                        // the pagefault handler may/may not be dependent on kmalloc.
                        // we're not going to assume it isn't.
                        int frame_id = pageframe_allocate_single(HEAP_PAGE_SET_ORDER);
                        if(frame_id == -1) {
                            panic("dynmem: no pageframes left for heap!\n");
                        }
                        
                        for(int j=frame_id*HEAP_PAGE_SET_SIZE;j<((frame_id+1)*HEAP_PAGE_SET_SIZE);j++) { // for all j from 2^order to (2^(order+1))-1...
                            size_t address = pageframe_get_block_addr(j, 0);
                            size_t vaddr = new_set + (0x1000*(j - (frame_id*HEAP_PAGE_SET_SIZE)));
                            paging_set_pte( vaddr, address, 0 );
                        }
                        allocator_sets[current_set] = new_set;
                    }
                    k_heap_header *set_start_block = (k_heap_header*)(allocator_sets[current_set]);
                    set_start_block->status = HEAP_HEADER_STATUS_USED;
                    
                    k_heap_header *allocation_start_block = (k_heap_header*)(allocator_sets[current_set]+length+sizeof(k_heap_header)+1);
                    allocation_start_block->status = HEAP_HEADER_STATUS_FREE;
                    
                    allocation_start_block->next = NULL;
                    set_start_block->next = allocation_start_block;
                    current->next = set_start_block;
                    current->status = HEAP_HEADER_STATUS_FREE;
                    return (void*)((size_t)set_start_block+sizeof(k_heap_header)+1);
                }
            }
        }
        // if we couldn't lock the block, then we just fall through to the next block
        current = current->next;
        current_set = next_set;
    }
    // if we're here, then we tried to allocate the last block in the list while it was locked.
    // if we can, try to restart the allocation after blocking for a bit.
    if((flags & 3) == KMALLOC_RESTART_ONCE) {
        process_switch_immediate();
        return kmalloc(init_length, (flags & 0x00FFFFFC) | KMALLOC_NO_RESTART);
    } else if( (flags & 3) == 3 ) { // KMALLOC_RESTART_MANY
        char restart_count = (flags>>24) & 0xFF
        restart_count--;
        if(restart_count > 0) {
            process_switch_immediate();
            return kmalloc(init_length, (flags & 0x00FFFFFF) | (restart_count<<24) | 3);
        } else {
            process_switch_immediate();
            return kmalloc(init_length, (flags & 0x00FFFFFC) | KMALLOC_RESTART_ONCE);
        }
    }
    // if we can't block (or if we've simply retried too many times), then just give up.
    return NULL;
}

void kfree(void* ptr) {
    k_heap_header *header = ((size_t)ptr-sizeof(k_heap_header)-1);
    if(header->status == HEAP_HEADER_STATUS_USED) {
        k_heap_header *iterate = heap_start;
        while( (iterate != NULL) && (iterate->next != header) )
            iterate = iterate->next;
        // iterate->next = header
        // or iterate == NULL in which case we're trying to free an invalid (unreachable) block
        if(iterate == NULL) {
            panic("dynmem: attempted to free an unreachable block!");
        }
        if( __sync_bool_compare_and_swap( header->next->status, HEAP_HEADER_STATUS_FREE, HEAP_HEADER_STATUS_USED ) ) {
            // header->next is free
            // delete header->next ( merge this block and the next )
            k_heap_header *next = header->next;
            header->next = next->next;
            
            next->status = 0;
            next->next = NULL;
        }
        // else next is not free
        
        if( __sync_bool_compare_and_swap( iterate->status, HEAP_HEADER_STATUS_FREE, HEAP_HEADER_STATUS_USED ) ) {
            // iterate is free
            // delete header ( merge the previous block and this one )
            iterate->next = header->next;
            
            header->status = 0;
            header->next = NULL;
            
            iterate->status = HEAP_HEADER_STATUS_FREE;
            return;
        }
        // else iterate is not free
        
        iterate->next->status = HEAP_HEADER_STATUS_FREE;
    } else {
        // if we're here, then either the pointer we initially assigned was a page-level allocation...
        // ...the pointer we were passed wasn't allocated with kmalloc at all...
        // ...or we're freeing a pointer twice.
        if( k_vmem_free((size_t)ptr) )
            return; // if the pointer wasn't returned from k_vmem_alloc, then nothing happens.
        // now we've ruled out the possibility of a page-level allocation
        // so that means that this pointer is invalid.
        panic("dynmem: attempted to free an invalid pointer!\nPointer points to: 0x%x.\n", (uint64_t)ptr);
    }
}