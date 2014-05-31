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
    kfree(ptr);
}

void operator delete[](void* ptr) {
    kfree(ptr);
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

void cpuid(uint32_t func_code, uint32_t *eax_ret, uint32_t *ebx_ret, uint32_t *ecx_ret, uint32_t *edx_ret) {
    asm volatile("cpuid"
                 : "=a"(*eax_ret), "=b"(*ebx_ret), "=c"(*ecx_ret), "=d"(*edx_ret)
                 : "a"(func_code));
}

void vendorid(uint32_t ret[3]) {
    asm volatile("mov $0, %%eax\n\t"
                 "cpuid\n\t"
                 : "=b"(ret[0]), "=d"(ret[1]), "=c"(ret[2])
                 : : "%eax");
}

/*
 * 64bit integer routines
 */
 
// 64-bit combined division / modulo operation
unsigned long long __udivmoddi4( uint64_t dividend, uint64_t divisor, uint64_t *remainder ) {
    if( divisor == 0 ) {
        return 5/(uint32_t)divisor;
    }
    
    // push the divisor all the way to the left
    uint64_t bit_to_add = 1;
    uint64_t quotient = 0;
    while( (divisor&(1<<63)) != 0 ) {
        divisor <<= 1;
        bit_to_add <<= 1;
    }
    
    while( bit_to_add > 0 ) {
        if( divisor <= dividend ) {
            dividend -= divisor;
            quotient += bit_to_add;
        }
        divisor >>= 1;
        bit_to_add >>= 1;
    }
    
    if( *remainder != NULL )
        (*remainder) = dividend;
    return quotient;
} 

unsigned long long __udivdi3( uint64_t dividend, uint64_t divisor ) {
    return __udivmoddi4( dividend, divisor, NULL );
}

unsigned long long __umoddi3( uint64_t dividend, uint64_t divisor ) {
    uint64_t ret = 0;
    __udivmoddi4( dividend, divisor, &ret );
    return ret;
}