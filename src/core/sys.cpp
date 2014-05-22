// sys.cpp - miscellaneous system functions

#include "includes.h"
#include "core/sys.h"
#include "core/dynmem.h"
#include "device/vga.h"

void *__stack_chk_guard = NULL;

void io_outb(short port, char data) {
    asm volatile("out %0, %1\n\t" : : "a"(data), "d"(port) );
    return;
}

void io_outw(short port, short data) {
    asm volatile("out %0, %1\n\t" : : "a"(data), "d"(port) );
    return;
}

void io_outd(short port, int data) {
    asm volatile("out %0, %1\n\t" : : "a"(data), "d"(port) );
    return;
}

char io_inb(short port) {
    char data;
    asm volatile("in %1, %0\n\t" : "=a"(data) : "d"(port) );
    return data;
}

short io_inw(short port) {
    short data;
    asm volatile("in %1, %0\n\t" : "=a"(data) : "d"(port) );
    return data;
}

int io_ind(short port) {
    int data;
    asm volatile("in %1, %0\n\t" : "=a"(data) : "d"(port) );
    return data;
}

void io_wait() {
    asm volatile("out %%al, $0x80" : : "a"(0));
}

uint64_t rdtsc() {
    uint64_t tsc;
    asm volatile("rdtsc" : "=A" (tsc));
    return tsc;
}

void *operator new(size_t size) {
    return kmalloc(size);
}

void *operator new[](size_t size) {
    return kmalloc(size);
}

void operator delete(void* ptr) {
    kfree((char*)ptr);
}

void operator delete[](void* ptr) {
    kfree((char*)ptr);
}

void memcpy(void* dst, void* src, size_t len) {
    char *d = (char*)dst;
    char *s = (char*)src;
    for(size_t i=0;i<len;i++)
        d[i] = s[i];
}

void memset(void* dst, char byte, size_t len) {
    char *d = (char*)dst;
    for(size_t i=0;i<len;i++)
        d[i] = byte;
}

void memclr(void* dst, size_t len) {
    char *d = (char*)dst;
    for(size_t i=0;i<len;i++)
        d[i] = 0;
}

void strcpy(char* dst, char* src, size_t len) {
    int i=0;
    while(true) {
        dst[i] = src[i];
        if( (src[i] == '\0') || ((len != 0) && (len == i) ) ) {
            break;
        }
        i++;
    }
}

bool strcmp(char* op1, char* op2, size_t len) {
    int i=0;
    while(true) {
        if( op1[i] != op2[i] ) {
            return false;
        }
        if( (op1[i] == '\0') || (op2[i] == '\0') || ((len != 0) && (len == i) ) ) {
            break;
        }
        i++;
    }
    return true;
}

// memory search
size_t memsrch(size_t start, size_t end, char *search_item, int len, size_t search_granularity) {
    for(size_t i=start;i<=end;i+=search_granularity) {
        char *data = (char*)start;
        if( data[0] == search_item[0] ) {
            bool is_match = true;
            for(int j=1;j<len;j++) {
                if(data[j] != search_item[j]) {
                    is_match = false;
                    break;
                }
            }
            if(is_match) {
                return i;
            }
        }
    }
    return 0xFFFFFFFF;
}

int pow(int base, int exp) {
    int ret = 1;
    for(int i=0;i<exp;i++)
        ret *= base;
    return ret;
}

uint64_t floor(double n) {
    uint64_t int_part = (uint64_t)n;
    return int_part;
}

uint64_t ceil(double n) {
    return floor(n)+1;
}

double fractional(double n) {
    return n - floor(n);
}

bool interrupts_enabled() {
    uint32_t eflags;
    asm volatile("pushf\n\t"
    "pop %0\n\t"
    : "=r"(eflags) : : "memory");
    return ((eflags & (1<<9)) > 0);
}