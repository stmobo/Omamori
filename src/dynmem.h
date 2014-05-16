//dynmem.h - header file for dynmem.cpp
#pragma once
#include "includes.h"

// size of a single block
#define HEAP_BLK_SIZE           128
// kernel heap start address
#define HEAP_START_ADDR         ((size_t)&kernel_end+1) // do *not* set to 0x100000, that's where the kernel goes
#define HEAP_MAGIC_NUMBER       0xA110CA7E

struct k_heap_blk {
    struct k_heap_blk  *next;
    struct k_heap_blk  *prev;
    bool                used;
    uint32_t            magic; // magic number, don't actually modify this
};

typedef struct k_heap_blk k_heap_blk;

struct k_heap {
    k_heap_blk   *start;
    k_heap_blk   *end;
};

typedef struct k_heap k_heap;

extern void k_heap_init(size_t);
extern char* kmalloc(size_t);
extern void kfree(char*);
extern void memblock_inspect();