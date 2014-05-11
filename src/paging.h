#pragma once
#include "multiboot.h"
#include "includes.h"

#define PAGEDIR_4MB_PAGE            (1<<7)
#define PAGEDIR_DIRTY               (1<<5)
#define PAGEDIR_CACHE_DISABLE       (1<<4)
#define PAGEDIR_WRITETHRU_CACHE     (1<<3)
#define PAGEDIR_USER_ACCESS         (1<<2)
#define PAGEDIR_WRITE_ACCESS        (1<<1)
#define PAGEDIR_IN_MEMORY               1

#define PAGETBL_GLOBAL              (1<<8)
#define PAGETBL_DIRTY               (1<<6)

// 1 order 8 block = 1MB
#define BUDDY_MAX_ORDER                 8

#define PAGING_BASE_ADDR            0x500

#define PAGING_KERNEL_BASE_ADDR     0xC0000000

//#define PAGING_DEBUG

typedef struct memory_range {
    unsigned long long int base;
    unsigned long long int end;
    unsigned long long int length;
    int page_index_start;
    int page_index_end;
    int n_pageframes;
} memory_range;

typedef struct pageframe {
    int id;
    int id_allocated_as;
    int order_allocated_as;
    size_t address;
} page_frame;

extern void initialize_pageframes(multiboot_info_t*);
extern void initialize_paging();
extern bool pageframe_get_block_status(int,int);
extern void pageframe_set_block_status(int,int,bool);
extern size_t get_block_addr(int,int);
extern int pageframe_get_block_from_addr(size_t);
extern page_frame* pageframe_allocate(int);
extern page_frame* pageframe_allocate_specific(int,int);
extern void pageframe_deallocate(page_frame*, int);
extern void pageframe_restrict_range(size_t, size_t);