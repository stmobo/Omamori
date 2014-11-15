// sys.cpp - miscellaneous system functions

#include "includes.h"
#include "core/sys.h"
#include "core/dynmem.h"
#include "device/vga.h"

void *__stack_chk_guard = NULL;

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
    uint8_t *d = (uint8_t*)dst;
    uint8_t *s = (uint8_t*)src;
    for(size_t i=0;i<len;i++)
        d[i] = s[i];
}

void memset(void* dst, uint8_t byte, size_t len) {
    uint8_t *d = (uint8_t*)dst;
    for(size_t i=0;i<len;i++)
        d[i] = byte;
}

void memclr(void* dst, size_t len) {
    uint8_t *d = (uint8_t*)dst;
    for(size_t i=0;i<len;i++)
        d[i] = 0;
}

void strcpy(char* dst, char* src, size_t len) {
    unsigned int i=0;
    while(true) {
        dst[i] = src[i];
        if( (src[i] == '\0') || ((len != 0) && (len == i) ) ) {
            break;
        }
        i++;
    }
}

bool strcmp(char* op1, char* op2, size_t len) {
    unsigned int i=0;
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
size_t memsrch(size_t start, size_t end, char *search_item, unsigned int len, size_t search_granularity) {
    for(size_t i=start;i<=end;i+=search_granularity) {
        char *data = (char*)start;
        if( data[0] == search_item[0] ) {
            bool is_match = true;
            for(unsigned int j=1;j<len;j++) {
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

/*
 * 64bit integer routines
 */
 
// 64-bit combined division / modulo operation
uint64_t __udivmoddi4( uint64_t dividend, uint64_t divisor, uint64_t *remainder ) {
    if( divisor == 0 ) {
        asm volatile("int $0");
        return 0;
    }
    
    // push the divisor all the way to the left
    uint64_t bit_to_add = 1;
    uint64_t quotient = 0;
    while( ((int64_t)divisor) >= 0 ) {
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
    
    if( remainder != NULL )
        (*remainder) = dividend;
    return quotient;
} 

uint64_t __udivdi3( uint64_t dividend, uint64_t divisor ) {
    return __udivmoddi4( dividend, divisor, NULL );
}

uint64_t __umoddi3( uint64_t dividend, uint64_t divisor ) {
    uint64_t ret = 0;
    __udivmoddi4( dividend, divisor, &ret );
    return ret;
}