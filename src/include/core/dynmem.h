//dynmem.h - header file for dynmem.cpp
#pragma once
#include "includes.h"

#define PAGE_SIZE                       0x1000
#define HEAP_MEMBLOCK_SIZE              32
#define HEAP_PAGE_SET_SIZE              128
// the order of 1 page set in the buddy allocator
// this is equal to log2(HEAP_PAGE_SET_SIZE)
#define HEAP_PAGE_SET_ORDER             7
#define HEAP_SET_SIZE                   (PAGE_SIZE*HEAP_PAGE_SET_SIZE)
#define HEAP_MAX_SETS                   1024
#define HEAP_SET_UNALLOCATED            0xFFFFFFFF
#define HEAP_SET0_START                 0xC0400000
#define HEAP_INITIAL_SETS_LENGTH        0x400000
// (1024 * 128) pages = 524288KB, or half of kernel space.
#define HEAP_HEADER_STATUS_FREE         0xDEA110C8
#define HEAP_HEADER_STATUS_USED         0xA110C8ED
#define KMALLOC_RESTART_COUNT                    5

#define KMALLOC_NO_RESTART              0x00000001
#define KMALLOC_RESTART_ONCE            0x00000002
// High byte determines the amount of restarting
#define KMALLOC_RESTART_MANY            (KMALLOC_RESTART_COUNT<<24) | 3

typedef struct k_heap_header {
    uint32_t status = HEAP_HEADER_STATUS_USED;
    k_heap_header *next = NULL;
} k_heap_header;

extern void k_heap_init();
extern "C" {
    extern void* kmalloc(size_t);
    extern void kfree(void*);
}
extern void* kmalloc(size_t, unsigned int);