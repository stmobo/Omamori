#include "arch/x86/multitask.h"
#include "device/vga.h"
#ifdef ENABLE_SERIAL_LOGGING
#include "device/serial.h"
#endif
#include "lib/sync.h"
#include "arch/x86/debug.h"
#include <stdarg.h>

const char* numeric_alpha = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

size_t strlen(char* str)
{
	size_t ret = 0;
	while ( str[ret] != 0 )
		ret++;
	return ret;
}

char* int_to_octal(long long int n) {
    if(n == 0) {
        char* mem = (char*)kmalloc(2);
        mem[0] = '0';
        mem[1] = '\0';
        return mem;
    }
    char *ret = (char*)kmalloc(24);
    for(int i=0;i<22;i++) {
        if( (n >> ((21-i)*3)) == 0 ) {
            ret[i] = '\0';
        }
        ret[i] = numeric_alpha[ ( ( n&(7<<((21-i)*3)) ) >> ((21-i)*3) ) ];
    }
    ret[22] = numeric_alpha[ ((n&(1LL<<63)) >> 63)&1 ];
    ret[23] = '\0';
    return ret;
}

char* int_to_hex(long long int n) {
    if(n == 0) {
        char* mem = (char*)kmalloc(2);
        mem[0] = '0';
        mem[1] = '\0';
        return mem;
    }
    int digits = 0;
    for(int i=0;i<16;i++) {
        if( (n >> (i*4)) == 0 ) {
            digits = i;
            break;
        }
    }
    char *ret = (char*)kmalloc(digits+1);
    for(int i=0;i<digits;i++) {
        ret[(digits-i)-1] = numeric_alpha[ ( (n >> i*4)&0xF ) ];
    }
    ret[digits] = '\0';
    return ret;
}

char* int_to_bin(long long int n) {
    if(n == 0) {
        char* mem = (char*)kmalloc(2);
        mem[0] = '0';
        mem[1] = '\0';
        return mem;
    }
    char *ret = (char*)kmalloc(65);
    for(int i=0;i<64;i++) {
        ret[i] = numeric_alpha[ (n>>(63-i))&0x1 ];
    }
    ret[64] = '\0';
    return ret;
}

// there is DEFINITELY a better way to do this.
char* int_to_decimal(signed long long int n) {
    size_t d = 0;
    int n2 = n;
    if(n == 0) {
        char* mem = (char*)kmalloc(2);
        mem[0] = '0';
        mem[1] = '\0';
        return mem;
    }
    while(n2 > 0) {
        d++;
        n2 /= 10;
    }
    n2 = n;
    d++;
    char* mem = (char*)kmalloc(d);
    for(int i=1;i<d;i++) {
        mem[(d-i)-1] = numeric_alpha[(n2%10)];
        n2 /= 10;
    }
    mem[d-1] = '\0';
    return mem;
}

char *concatentate_strings(char* str1, char *str2) {
    int len = strlen(str1) + strlen(str2);
    char *out = (char*)kmalloc(len + 2);
    for(size_t i=0;i<strlen(str1);i++) {
        out[i] = str1[i];
    }
    for(size_t i=0;i<strlen(str2);i++) {
        out[ i+strlen(str1) ] = str2[i];
    } 
    out[len+1] = '\0';
    return out;
}

char *append_char(char* str1, char c) {
    char *out = (char*)kmalloc( strlen(str1)+2 );
    for(unsigned int i=0;i<strlen(str1);i++) {
        out[i] = str1[i];
    }
    out[strlen(str1)] = c;
    out[strlen(str1)+1] = 0;
    return out;
}

// Print to string
char *ksprintf_varg(const char* str, va_list args) {
    char *out = (char*)kmalloc(1);
    out[0] = '\0';
    for( size_t i=0;i<strlen(const_cast<char*>(str));i++ ) {
        if( str[i] == '%' ) {
            switch( str[i+1] ) {
                case 'u':
                {
                    char *str2 = int_to_decimal( va_arg(args, unsigned long long int) );
                    char *out_copy = concatentate_strings( out, str2 );
                    kfree(out);
                    out = out_copy;
                    i++;
                    break;
                }
                case 'd':
                case 'i':
                {
                    signed long long int silli = va_arg(args, signed long long int);
                    char *str2 = int_to_decimal( silli );
                    if(silli < 0) {
                        str2 = concatentate_strings( "-", str2 );
                        silli *= -1;
                    }
                    char *out_copy = concatentate_strings( out, str2 );
                    kfree(out);
                    out = out_copy;
                    i++;
                    break;
                }
                case 'o':
                {
                    char *str2 = int_to_octal( va_arg(args, long long int) );
                    char *out_copy = concatentate_strings( out, str2 );
                    kfree(out);
                    out = out_copy;
                    i++;
                    break;
                }
                case 'p':
                    {
                        void *ptr = va_arg(args, void*);
                        char *upper = int_to_hex( reinterpret_cast<size_t>(ptr) );
                        for(int i=0;i<16;i++) {
                            switch(upper[i]) {
                                case 'A':
                                    upper[i] = 'a';
                                    break;
                                case 'B':
                                    upper[i] = 'b';
                                    break;
                                case 'C':
                                    upper[i] = 'c';
                                    break;
                                case 'D':
                                    upper[i] = 'd';
                                    break;
                                case 'E':
                                    upper[i] = 'e';
                                    break;
                                case 'F':
                                    upper[i] = 'f';
                                    break;
                                default:
                                    break;
                            }
                            if(upper[i] == 0) {
                                break;
                            }
                        }
                        char *out_copy = concatentate_strings( out, upper );
                        kfree(out);
                        out = out_copy;
                        i++;
                        break;
                    }
                case 'x':
                    {
                        char *upper = int_to_hex( va_arg(args, long long int) );
                        for(int i=0;i<16;i++) {
                            switch(upper[i]) {
                                case 'A':
                                    upper[i] = 'a';
                                    break;
                                case 'B':
                                    upper[i] = 'b';
                                    break;
                                case 'C':
                                    upper[i] = 'c';
                                    break;
                                case 'D':
                                    upper[i] = 'd';
                                    break;
                                case 'E':
                                    upper[i] = 'e';
                                    break;
                                case 'F':
                                    upper[i] = 'f';
                                    break;
                                default:
                                    break;
                            }
                            if(upper[i] == 0) {
                                break;
                            }
                        }
                        char *out_copy = concatentate_strings( out, upper );
                        kfree(out);
                        out = out_copy;
                        i++;
                        break;
                    }
                case 'X':
                {
                    char *str2 = int_to_hex( va_arg(args, long long int) );
                    char *out_copy = concatentate_strings( out, str2 );
                    kfree(out);
                    out = out_copy;
                    i++;
                    break;
                }
                case 'b':
                {
                    char *str2 = int_to_bin( va_arg(args, long long int) );
                    char *out_copy = concatentate_strings( out, str2 );
                    kfree(out);
                    out = out_copy;
                    i++;
                    break;
                }
                case 's':
                {
                    char *str2 = va_arg(args, char*);
                    char *out_copy = concatentate_strings( out, str2 );
                    kfree(out);
                    out = out_copy;
                    i++;
                    break;
                }
                case 'c':
                {
                    char c = va_arg(args, int);
                    char c2[2];
                    c2[0] = c;
                    c2[1] = 0;
                    char *out_copy = concatentate_strings( out, c2 );
                    kfree(out);
                    out = out_copy;
                    i++;
                    break;
                }
                case '%':
                {
                    char *out_copy = append_char( out, '%' );
                    kfree(out);
                    out = out_copy;
                    i++;
                    break;
                }
            }
        } else {
            char *out_copy = append_char( out, str[i] );
            kfree(out);
            out = out_copy;
        }
    }
    return out;
}

// Format something.
char *ksprintf(const char *str, ...) {
    va_list args;
    va_start(args, str);
    char *out = ksprintf_varg(str, args);
    va_end(args);
    return out;
}

spinlock __kprintf_lock;
//static spinlock __kprintf_varg_lock;
// Print something to the screen, but take a va_list.
void kprintf_varg(const char *str, va_list args) {
    char *o = ksprintf_varg( str, args );
    __kprintf_lock.lock();
#ifdef ENABLE_SERIAL_LOGGING
    if( serial_initialized ) {
        serial_write( o );
    }
#endif
    terminal_writestring( o );
    __kprintf_lock.unlock();
}

// Print something to screen.
// DO NOT CALL THIS FROM AN ISR!!!
void kprintf(const char *str, ...) {
    va_list args;
    va_start(args, str);
    
    char *o = ksprintf_varg( str, args );
    __kprintf_lock.lock();
#ifdef ENABLE_SERIAL_LOGGING
    if( serial_initialized ) {
        serial_write( o );
    }
#endif
    terminal_writestring( o );
    __kprintf_lock.unlock();
    
    va_end(args);
}

char *panic_str = NULL;
bool double_panic = false;

// Print "panic: mesg" and then hang.
void panic(char *str, ...) {
    // Disable multitasking (and spinlocks too).
    multitasking_enabled = 0;
    if(double_panic) { // triple panic / fault!
        panic_str = str;
        while(true) {
            asm volatile("cli\n\t"
            "hlt"
            : : : "memory");
        }
    }
    if(panic_str != NULL) { // Don't panic WHILE panicking.
        double_panic = true;
        terminal_writestring("panic: panicked while panicking!\n");
        terminal_writestring("first panic string: ");
        terminal_writestring(panic_str);
        terminal_writestring("\nsecond panic string: ");
        terminal_writestring(str);
    } else {
        panic_str = str; // save the unformatted string just in case
        
        va_list args;
        va_start(args, str);
        
        char *o = ksprintf_varg( str, args );
        panic_str = o; // okay, save the formatted string since we now have it.
        
        terminal_writestring("panic: ");
        terminal_writestring(o);
        va_end(args);
        // (don't) Get a stack trace.
        //stack_trace_walk(7);
    }
    // Now halt.
    while(true) {
        asm volatile("cli\n\t"
        "hlt"
        : : : "memory");
    }
}