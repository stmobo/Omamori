// dynmem.cpp - kernel heap allocation
// this is supposed to be available from early init on.
#include "includes.h"
#include "core/sys.h"
#include "core/dynmem.h"
#include "device/vga.h"

k_heap heap;

// k_heap_init - initialize the heap
// This function creates the initial heap block struct, and also initializes the k_heap struct.
void k_heap_init(size_t heap_addr) {
    heap.start = (k_heap_blk*)heap_addr;
    heap.end   = (k_heap_blk*)heap_addr;
    heap.start->prev = NULL;
    heap.start->next = NULL;
    heap.start->used = false;
    heap.start->magic = HEAP_MAGIC_NUMBER;
}

// k_heap_get - Get an entry in the linked list.
// This function traverses the linked list, returning a pointer to the nth block.
k_heap_blk* k_heap_get(int n) {
    k_heap_blk* blk = heap.start;
    for(int i=0;i<n;i++) {
        blk = blk->next;
    }
    return blk;
}

// k_heap_add_at_offset - Add a new heap block
// This function places a new heap block in memory, linked to an "origin" block.
void k_heap_add_at_offset(k_heap_blk* origin_blk, int block_offset) {
    k_heap_blk* blk = (k_heap_blk*)((size_t)origin_blk+(block_offset*HEAP_BLK_SIZE));
    blk->prev = origin_blk;
    blk->next = origin_blk->next;
    blk->magic = HEAP_MAGIC_NUMBER;
    
    blk->prev->next = blk;
    if(blk->next != NULL)
        blk->next->prev = blk;
    else
        heap.end = blk;
}

// k_heap_delete - Unlink a block
// This function removes a block from the linked list, effectively "deleting" it.
void k_heap_delete(k_heap_blk* blk) {
    if(blk == NULL) {
        panic("dynmem: Attempted to delete NULL block!");
    }
    if(blk->prev != NULL) {
        blk->prev->next = blk->next;
    } else {
        return; // can't delete the first block
    }
    if(blk->next != NULL) {
        blk->next->prev = blk->prev;
    } else {
        heap.end = blk->prev;
    }
    blk->next = NULL;
    blk->prev = NULL;
    blk->magic = 0;
    blk->used = false;
}

// k_heap_compress - Merge free blocks
// This function "compresses" the linked list, consolidating as many adjacent free blocks as possible, to reduce fragmentation.
int k_heap_compress() {
    k_heap_blk* blk = heap.start;
    k_heap_blk* next = NULL;
    int blks_deleted = 0;
    
    while((blk != NULL) && (blk->next != NULL)) { // Don't compress the first or last block in the heap.
        next = blk->next;
        if(blk->prev != NULL) {
            if(!blk->used && !blk->prev->used) { // If both the current block and the one before it are free...
                // ...then _delete_ this block. 
                k_heap_delete(blk);
                blks_deleted++;
            }
        }
        blk = next;
    }
    
    if((heap.end->prev != NULL) && (!heap.end->prev->used)) {
        k_heap_delete(heap.end);
        blks_deleted++;
    }

    return blks_deleted;
}

// kmalloc - kernel memory allocator
// This function returns a pointer to an arbitrary size block of memory.
char* kmalloc(size_t size) {
    // iterate over every block in the heap list except for the last one
    // and look for a block where the space between the block and the next in the list is at least size bytes...
    // if we can't find one, extend the heap.
    int n_blks = (size / HEAP_BLK_SIZE)+1;
    char* ptr = NULL;
    k_heap_blk* blk = heap.start;
    while(blk->next != NULL) {
        if(!blk->used) {
            int blk_sz = ((size_t)blk->next - (size_t)blk);
            if( blk_sz-sizeof(k_heap_blk) >= size ) {
                blk->used = true;
                char* ptr = NULL;
                if( (blk_sz / HEAP_BLK_SIZE) > n_blks ) {
                    k_heap_add_at_offset(blk, n_blks);
                    blk->next->used = false;
                    ptr = (char*)((size_t)blk+sizeof(k_heap_blk)+1);
                } else {
                    ptr = (char*)((size_t)blk+sizeof(k_heap_blk)+1);
                }
                break;
            }
        }
        blk = blk->next;
    }
    if(ptr == NULL) { // if we still haven't allocated memory, then make a new block.
        k_heap_add_at_offset(heap.end, n_blks);
        heap.end->prev->used = true;
        ptr = (char*)((size_t)(heap.end->prev)+sizeof(k_heap_blk)+1);
    }
    return ptr;
}

// kfree - free memory block
// This function frees a block of memory given by kmalloc(), allowing other kernel tasks to use it.
void kfree(char* ptr) {
    // given a pointer to a block of memory:
    // find the header for that block of memory
    // set the free bit
    // compress the list
    k_heap_blk *header_ptr = (k_heap_blk*)((size_t)(ptr-sizeof(k_heap_blk)-1));
    if(header_ptr->magic == HEAP_MAGIC_NUMBER) {
        header_ptr->used = false;
        if(header_ptr->prev != NULL) {
            if( !(header_ptr->prev->used) ) {
                // Just delete this block.
                k_heap_delete(header_ptr);
            } 
        }
        if(header_ptr->next != NULL) {
            if( !(header_ptr->next->used) ) {
                // Delete header_ptr->next.
                k_heap_blk *next = header_ptr->next;
                k_heap_delete(next);
            }
        }
        
        k_heap_compress();
    } else {
        // We're freeing an invalid pointer.
        panic("dynmem: bad free() call -- could not find magic number");
    }
    
}

// memblock_inspect - print linked list overview
// This function traverses the list and prints debugging info (struct data members) to console.
void memblock_inspect() {
#ifdef DEBUG
    k_heap_blk* blk = heap.start;
    while((blk != NULL)) { // Don't compress the first or last block in the heap.
        kprintf("Heap block located at 0x%x:\nblk->prev: 0x%x\nblk->next: 0x%x", (size_t)blk, (size_t)blk->prev, (size_t)blk->next);
        if(blk->used)
            terminal_writestring("\nBlock marked as \'used\'.\n");
        else
            terminal_writestring("\nBlock marked as \'free\'.\n");
        blk = blk->next;
    }
#endif
}