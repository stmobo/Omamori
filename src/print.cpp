#include "vga.h"
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
    char *ret = kmalloc(24);
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
    char *ret = kmalloc(17);
    for(int i=0;i<16;i++) {
        if( (n >> ((15-i)*4)) == 0 ) {
            ret[i] = '\0';
        }
        ret[i] = numeric_alpha[ ( ( n&(0xF<<((15-i)*4)) ) >> ((15-i)*4) ) ];
    }
    ret[16] = '\0';
    return ret;
}

char* int_to_bin(long long int n) {
    char *ret = kmalloc(65);
    for(int i=0;i<64;i++) {
        ret[i] = numeric_alpha[ (n>>(63-i))&0x1 ];
    }
    ret[64] = '\0';
    return ret;
}

// there is DEFINITELY a better way to do this.
char* int_to_decimal(signed long long int n) {
    int d = 0;
    int n2 = n;
    if(n == 0) {
        char* mem = kmalloc(2);
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
    char* mem = kmalloc(d);
    for(int i=1;i<d;i++) {
        mem[(d-i)-1] = numeric_alpha[(n2%10)];
        n2 /= 10;
    }
    mem[d-1] = '\0';
    return mem;
}


void kprintf(const char* str, ...) {
    va_list args;
    va_start(args, str);
    for( size_t i=0;i<strlen(const_cast<char*>(str));i++ ) {
        if( str[i] == '%' ) {
            switch( str[i+1] ) {
                case 'u':
                    {
                        char *str = int_to_decimal( va_arg(args, unsigned long long int) );
                        terminal_writestring( str );
                        i++;
                        break;
                    }
                case 'd':
                case 'i':
                    {
                        signed long long int silli = va_arg(args, signed long long int);
                        if(silli < 0) {
                            terminal_putchar('-');
                            silli *= -1;
                        }
                        terminal_writestring( int_to_decimal( silli ) );
                        i++;
                        break;
                    }
                case 'o':
                    terminal_writestring( int_to_octal( va_arg(args, long long int) ) );
                    i++;
                    break;
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
                        terminal_writestring(upper);
                        i++;
                        break;
                    }
                case 'X':
                    terminal_writestring( int_to_hex( va_arg(args, long long int) ) );
                    i++;
                    break;
                case 'b':
                    terminal_writestring( int_to_bin( va_arg(args, long long int) ) );
                    i++;
                    break;
                case 's':
                    terminal_writestring( va_arg(args, char*) );
                    i++;
                    break;
                case 'c':
                    terminal_putchar( (char)va_arg(args, int) );
                    i++;
                    break;
                case '%':
                    terminal_putchar( '%' );
                    i++;
                    break;
            }
        } else {
            terminal_putchar( str[i] );
        }
    }
    va_end(args);
}