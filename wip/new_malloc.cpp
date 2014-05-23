// new_malloc.cpp
// A newer malloc for a newer kernel.

#define HEAP_MEMBLOCK_SIZE              32
#define HEAP_HEADER_STATUS_FREE         0xDEA110C8
#define HEAP_HEADER_STATUS_USED         0xA110C8ED

k_heap_header *heap_start;

struct k_heap_header = {
    uint32_t status = HEAP_HEADER_STATUS_USED;
    k_heap_header *next = NULL;
}

void *kmalloc(size_t length) {
    k_heap_header *current = heap_start;
    if(length < HEAP_MEMBLOCK_SIZE)
        length = HEAP_MEMBLOCK_SIZE;
    // find the nearest multiple of the block size
    length = ((length - (length % HEAP_MEMBLOCK_SIZE)) + HEAP_MEMBLOCK_SIZE);
    if( length > 0x1000 ) {
        // allocate entire pages
    }
    while( current != NULL ) {
        if( current->next != NULL ) {
            if( __sync_bool_compare_and_swap( current->status, HEAP_HEADER_STATUS_FREE, HEAP_HEADER_STATUS_USED ) ) {
                // using an atomic CAS both locks the block and saves us the trouble of marking it as "used" if we /do/ use it
                size_t block_len = (current->next - (current+sizeof(k_heap_header)));
                if( ((size_t)current & 0xFFFFF000) != (current->next & 0xFFFFF000) ) {
                    // this block and the next are on different pages
                    // so we need to adjust for that
                    block_len = ( (((size_t)current & 0xFFFFF000) + 0xFFF) - (size_t)current);
                }
                if( block_len < length ) { // can't use this block
                    current->status = HEAP_HEADER_STATUS_FREE;
                    // fall through
                } else if( block_len > length ) {
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
            }
        } else {
            // okay so we need to add a new block to the list
            // where that block is is a matter of how far we are into the page
            size_t end_of_page = ((size_t)current & 0xFFFFF000) + 0xFFF;
            size_t avail_len = (end_of_page - (size_t)current);
            if( (avail_len - length) >= sizeof(k_heap_header)+HEAP_MEMBLOCK_SIZE ) { // can we put another block down?
                if( __sync_bool_compare_and_swap( current->status, HEAP_HEADER_STATUS_FREE, HEAP_HEADER_STATUS_USED ) ) {
                    // maintain reentrancy(?)
                    k_heap_header *new_block = ((size_t)current+(length+sizeof(k_heap_header))+1);
                    new_block->next = NULL;
                    new_block->status = HEAP_HEADER_STATUS_FREE;
                    current->next = new_block;
                    return (void*)((size_t)current+sizeof(k_heap_header)+1);
                } else {
                    // something else is already allocating this block, what do?
                }
            } else {
                // not enough space left on the current page
                // get another one
                if( __sync_bool_compare_and_swap( current->status, HEAP_HEADER_STATUS_FREE, HEAP_HEADER_STATUS_USED ) ) {
                    size_t new_heap_page = k_vmem_alloc(1);
                    int frame_id = pageframe_allocate_single();
                    if( new_heap_page == -1 || frame_id == -1 ) {
                        // we couldn't allocate a new frame for the heap.
                        panic("dynmem: No free frames left for kernel heap!\n");
                    }
                    paging_set_pte( new_heap_page, pageframe_get_block_addr(frame_id, 0), 0 );
                    
                    k_heap_header *page_start = (k_heap_header*)new_heap_page;
                    k_heap_header *alloc_start = (k_heap_header*)(new_heap_page+length+sizeof(k_heap_header)+1);
                    
                    page_start->status = HEAP_HEADER_STATUS_USED;
                    page_start->next = alloc_start;
                    
                    alloc_start->status = HEAP_HEADER_STATUS_FREE;
                    alloc_start->next = NULL;
                    
                    current->next = page_start;
                    return (void*)(new_heap_page+sizeof(k_heap_header)+1);
                } else {
                    // something else is allocating this block.
                }
            }
        }
        current = current->next;
    }
    return NULL;
}

void kfree(void* ptr) {
    k_heap_header *header = ((size_t)ptr-sizeof(k_heap_header)-1);
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
}