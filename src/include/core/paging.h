#pragma once
#include "includes.h"
#include "boot/multiboot.h"
#include "lib/sync.h"

// 1 order 8 block = 1MB
#define BUDDY_MAX_ORDER                 8

#define PAGING_BASE_ADDR            0x500

#define PAGING_KERNEL_BASE_ADDR     0xC0000000
//#define PAGING_DEBUG
#define PAGEFAULT_DEBUG

#define HEAP_INITIAL_ALLOCATION     0x400000
#define HEAP_INITIAL_PHYS_ADDR      0x401000
#define HEAP_INITIAL_PT_ADDR        (HEAP_INITIAL_PHYS_ADDR+HEAP_INITIAL_ALLOCATION+0x1000)

typedef struct memory_range {
    phys_addr_t base;
    phys_addr_t end;
    phys_diff_t length;
    int page_index_start;
    int page_index_end;
    int n_pageframes;
} memory_range;

typedef struct pageframe {
    int id;
    int id_allocated_as;
    int order_allocated_as;
    phys_addr_t address;
} page_frame;

#define AVL_ORDERING_ELEMENT length
struct vaddr_range {
    virt_addr_t address;
    bool free;
    struct vaddr_range *next;
    struct vaddr_range *prev;
};

typedef struct vaddr_range vaddr_range;

extern unsigned long long int mem_avail_bytes;
extern int mem_avail_kb;
extern int num_pages;
extern memory_map_t* mem_map;
extern size_t mem_map_len;
extern int n_mem_ranges;

extern memory_range* memory_ranges;
extern size_t **buddy_maps;
extern int n_blocks[BUDDY_MAX_ORDER+1];

extern vaddr_range k_vmem_linked_list;
extern vaddr_range __k_vmem_allocate_start;

extern uint32_t initial_heap_pagetable[1024] __attribute__((aligned(0x1000)));
extern uint32_t global_kernel_page_directory[256]; // spans PDE nos. 768 - 1023

extern bool pageframes_initialized;

// various asm-exported stuff
extern uint32_t *PageTable0;
extern uint32_t *PageTable768;
extern uint32_t *BootPD; // see early_boot.s

// initialization
extern void initialize_vmem_allocator();
extern void initialize_pageframes(multiboot_info_t*);

// pageframe allocator stuff
extern bool pageframe_get_block_status(int,int);
extern void pageframe_set_block_status(int,int,bool);
extern phys_addr_t pageframe_get_block_addr(int,int);
extern int pageframe_get_alloc_order(int);
extern int pageframe_get_block_from_addr(phys_addr_t);

// pageframe allocation/deallocation functions
extern page_frame* pageframe_allocate(int);
extern page_frame* pageframe_allocate_at( phys_addr_t, int );
extern page_frame* pageframe_allocate_specific(int,int);
extern int pageframe_allocate_single(int);
extern void pageframe_deallocate(page_frame*, int);
extern void pageframe_deallocate_specific(int, int);

// vmem allocation with arbitrary ranges
extern virt_addr_t paging_vmem_alloc( vaddr_range*, virt_addr_t, int );
extern virt_addr_t paging_vmem_alloc_specific( vaddr_range*, virt_addr_t, virt_addr_t );
extern bool paging_vmem_free( vaddr_range*, virt_addr_t );

// vmem allocation with the kernel allocator
extern virt_addr_t k_vmem_alloc( int );
extern virt_addr_t k_vmem_alloc( virt_addr_t, virt_addr_t );
extern virt_addr_t k_vmem_free( virt_addr_t );

// misc. pageframe allocator functions
extern void pageframe_restrict_range(size_t, size_t);

// page table modification functions
extern inline void invalidate_tlb(virt_addr_t);
extern void paging_set_pte(virt_addr_t, phys_addr_t, uint16_t);
extern uint32_t paging_get_pte(virt_addr_t);
extern void paging_unset_pte(virt_addr_t);

// combination vmem/pmem allocation functions
extern virt_addr_t paging_map_phys_address( phys_addr_t, int );
extern void paging_unmap_phys_address( phys_addr_t, int );
extern virt_addr_t mmap(int);
extern void munmap(size_t);

// misc.
extern void copy_pageframe_range( phys_addr_t, phys_addr_t, int );
extern page_frame* duplicate_pageframe_range( phys_addr_t, int );
#ifdef __x86__
inline void invalidate_tlb(size_t address) {
    asm volatile("invlpg (%0)" : : "r"(address) : "memory");
}
#endif
// add a def for ARM here
