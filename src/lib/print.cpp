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

char *printf_form_final_str( va_list args, char specifier,
    char type_length, int min_width, int precision,
    bool pad_left, bool pad_zeroes, bool add_sign,
    bool add_blank, bool add_identifier ) 
    {
    
    bool is_negative = false;
    char *str2 = NULL;
    switch( specifier ){
        case 'c':
        {
            char c = va_arg(args, int);
            str2 = (char*)kmalloc(2);
            str2[0] = c;
            str2[1] = 0;
            return str2;
        }
        case 's':
        {
            char *str3 = va_arg(args, char*);
            return str3;
        }
        case 'u':
        {
            switch( type_length ) {
                case 'q':
                    str2 = int_to_decimal( (unsigned char)va_arg(args, int) );
                    break;
                case 'h':
                    str2 = int_to_decimal( (unsigned short int)va_arg(args, int) );
                    break;
                case 'l':
                    str2 = int_to_decimal( va_arg(args, unsigned long int) );
                    break;
                case 'b':
                    str2 = int_to_decimal( va_arg(args, unsigned long long int) );
                    break;
                case 'j':
                    str2 = int_to_decimal( va_arg(args, uintmax_t) );
                    break;
                case 'z':
                    str2 = int_to_decimal( va_arg(args, size_t) );
                    break;
                case 't':
                    str2 = int_to_decimal( va_arg(args, ptrdiff_t) );
                    break;
                case 0:
                default:
                    str2 = int_to_decimal( va_arg(args, unsigned int) );
                    break;
            }
            break;
        }
        case 'o':
        {
            switch( type_length ) {
                case 'q':
                    str2 = int_to_octal( (unsigned char)va_arg(args, int) );
                    break;
                case 'h':
                    str2 = int_to_octal( (unsigned short int)va_arg(args, int) );
                    break;
                case 'l':
                    str2 = int_to_octal( va_arg(args, unsigned long int) );
                    break;
                case 'b':
                    str2 = int_to_octal( va_arg(args, unsigned long long int) );
                    break;
                case 'j':
                    str2 = int_to_octal( va_arg(args, uintmax_t) );
                    break;
                case 'z':
                    str2 = int_to_octal( va_arg(args, size_t) );
                    break;
                case 't':
                    str2 = int_to_octal( va_arg(args, ptrdiff_t) );
                    break;
                case 0:
                default:
                    str2 = int_to_octal( va_arg(args, unsigned int) );
                    break;
            }
            break;
        }
        case 'X':
        {
            switch( type_length ) {
                case 'q':
                    str2 = int_to_hex( (unsigned char)va_arg(args, int) );
                    break;
                case 'h':
                    str2 = int_to_hex( (unsigned short int)va_arg(args, int) );
                    break;
                case 'l':
                    str2 = int_to_hex( va_arg(args, unsigned long int) );
                    break;
                case 'b':
                    str2 = int_to_hex( va_arg(args, unsigned long long int) );
                    break;
                case 'j':
                    str2 = int_to_hex( va_arg(args, uintmax_t) );
                    break;
                case 'z':
                    str2 = int_to_hex( va_arg(args, size_t) );
                    break;
                case 't':
                    str2 = int_to_hex( va_arg(args, ptrdiff_t) );
                    break;
                case 0:
                default:
                    str2 = int_to_hex( va_arg(args, unsigned int) );
                    break;
            }
            break;
        }
        case 'p':
        {
            void *ptr = va_arg(args, void*);
            str2 = int_to_hex( reinterpret_cast<uint32_t>(ptr) );
            break;
        }
        case 'x':
        {
            switch( type_length ) {
                case 'q':
                    str2 = int_to_hex( (unsigned char)va_arg(args, int) );
                    break;
                case 'h':
                    str2 = int_to_hex( (unsigned short int)va_arg(args, int) );
                    break;
                case 'l':
                    str2 = int_to_hex( va_arg(args, unsigned long int) );
                    break;
                case 'b':
                    str2 = int_to_hex( va_arg(args, unsigned long long int) );
                    break;
                case 'j':
                    str2 = int_to_hex( va_arg(args, uintmax_t) );
                    break;
                case 'z':
                    str2 = int_to_hex( va_arg(args, size_t) );
                    break;
                case 't':
                    str2 = int_to_hex( va_arg(args, ptrdiff_t) );
                    break;
                case 0:
                default:
                    str2 = int_to_hex( va_arg(args, unsigned int) );
                    break;
            }
            for(int i=0;i<strlen(str2);i++) {
                switch(str2[i]) {
                    case 'A':
                        str2[i] = 'a';
                        break;
                    case 'B':
                        str2[i] = 'b';
                        break;
                    case 'C':
                        str2[i] = 'c';
                        break;
                    case 'D':
                        str2[i] = 'd';
                        break;
                    case 'E':
                        str2[i] = 'e';
                        break;
                    case 'F':
                        str2[i] = 'f';
                        break;
                    default:
                        break;
                }
                if(str2[i] == 0) {
                    break;
                }
            }
            break;
        } // case 'x'
        case 'f':
        case 'F':
        {
            double n = va_arg(args, double);
            double frac = fractional(n);
            int whole = floor(n);
            int frac_int = 0;
            while( floor(frac) != frac ) {
                frac *= 10;
            }
            frac_int = frac;
            
            char *whole_str = int_to_decimal(whole);
            char *frac_str = int_to_decimal(frac_int);
            
            str2 = concatentate_strings( whole_str, concatentate_strings( ".", frac_str ) );
            kfree( whole_str );
            kfree( frac_str );
            break;
        }
        case 'd':
        case 'i':
        {
            switch( type_length ) {
                case 'q':
                {
                    signed char n = va_arg(args, int);
                    if( n < 0 ){
                        is_negative = true;
                        n *= -1;
                    }
                    str2 = int_to_decimal( n );
                    break;
                }
                case 'h':
                {
                    short int n = va_arg(args, int);
                    if( n < 0 ){
                        is_negative = true;
                        n *= -1;
                    }
                    str2 = int_to_decimal( n );
                    break;
                }
                case 'l':
                {
                    long int n = va_arg(args, long int);
                    if( n < 0 ){
                        is_negative = true;
                        n *= -1;
                    }
                    str2 = int_to_decimal( n );
                    break;
                }
                case 'b':
                {
                    long long n = va_arg(args, long long int);
                    if( n < 0 ){
                        is_negative = true;
                        n *= -1;
                    }
                    str2 = int_to_decimal( n );
                    break;
                }
                case 'j':
                {
                    intmax_t n = va_arg(args, intmax_t);
                    if( n < 0 ){
                        is_negative = true;
                        n *= -1;
                    }
                    str2 = int_to_decimal( n );
                    break;
                }
                case 'z':
                {
                    size_t n = va_arg(args, size_t);
                    if( n < 0 ){
                        is_negative = true;
                        n *= -1;
                    }
                    str2 = int_to_decimal( n );
                    break;
                }
                case 't':
                {
                    ptrdiff_t n = va_arg(args, ptrdiff_t);
                    if( n < 0 ){
                        is_negative = true;
                        n *= -1;
                    }
                    str2 = int_to_decimal( n );
                    break;
                }
                case 0:
                default:
                {
                    int n = va_arg(args, int);
                    if( n < 0 ){
                        is_negative = true;
                        n *= -1;
                    }
                    str2 = int_to_decimal( n );
                    break;
                }
            }
            break;
        } // case 'd' / 'i'
        
    }
    if( str2 == NULL )
        return NULL;
    if(min_width > strlen(str2)) {
        int n_padding = strlen(str2) - min_width;
        for(int i=0;i<n_padding;i++) {
            if( !pad_left ) {
                char* str2_copy = NULL;
                if( pad_zeroes )
                    str2_copy = concatentate_strings( "0", str2 );
                else
                    str2_copy = concatentate_strings( " ", str2 );
                if( str2_copy == NULL )
                    return NULL;
                kfree(str2);
                str2 = str2_copy;
            } else {
                char* str2_copy = NULL;
                if( pad_zeroes )
                    str2_copy = concatentate_strings( str2, "0" );
                else
                    str2_copy = concatentate_strings( str2, " ");
                if( str2_copy == NULL )
                    return NULL;
                kfree(str2);
                str2 = str2_copy;
            }
        }
    }
    char* str2_copy = NULL;
    if( is_negative ) {
        str2_copy = concatentate_strings( "-", str2 );
    } else if( add_sign ) {
        str2_copy = concatentate_strings( "+", str2 );
    } else if( add_blank ) {
        str2_copy = concatentate_strings( " ", str2 );
    }
    if( is_negative || add_sign || add_blank ){
        if( str2_copy == NULL )
            return NULL;
        kfree(str2);
        str2 = str2_copy;
    }
    return str2;
}

// Print to string
char *ksprintf_varg(const char* str, va_list args) {
    char *out = (char*)kmalloc(1);
    out[0] = '\0';
    
    bool reading_specifier = false;
    
    // Flags
    bool pad_left = false; // if (min_width > strlen(int_to_<whatever>(next_arg)) then pad
    bool use_zeroes = false; // ..for padding (see above)
    bool always_sign = false;
    bool blank_sign = false;
    bool add_punctuation = false; // add '0', '0x', or decimal points depending on output format
    
    char length = 0; // 'q' instead of 'hh' (signed char) and 'b' instead of 'll' (long long)
    
    int min_width = 0;
    int precision = 0;
    bool reading_precision = false;
    bool reading_width = false;
    
    for( size_t i=0;i<strlen(const_cast<char*>(str));i++ ) {
        if( str[i] == '%' ) {
            if( str[i+1] == '%' ) {
                char *out_copy = append_char( out, '%' );
                kfree(out);
                out = out_copy;
                i++;
            } else {
                reading_specifier = true;
            }
        } else if(reading_specifier) {
            switch( str[i] ) {
                case '0': // width / precision fields
                    if( !(reading_precision || reading_width) ) {
                        use_zeroes = true;
                        break;
                    }
                    // else fall through
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                    if(reading_precision) {
                        precision *= 10;
                        for(int j=0;j<10;j++) {
                            if( str[i] == numeric_alpha[j] ) {
                                precision += j;
                                break;
                            }
                        }
                    } else {
                        reading_width = true;
                        min_width *= 10;
                        for(int j=0;j<10;j++) {
                            if( str[i] == numeric_alpha[j] ) {
                                min_width += j;
                                break;
                            }
                        }
                    }
                    break;
                case '.':
                    reading_precision = true;
                    break;
                case '*':
                    if(reading_precision) {
                        precision = va_arg(args, unsigned int);
                    } else {
                        min_width = va_arg(args, unsigned int);
                    }
                    break;
                case '-': // flags
                    pad_left = true;
                    break;
                case '+':
                    always_sign = true;
                    break;
                case ' ':
                    blank_sign = true;
                    break;
                case '#':
                    add_punctuation = true;
                    break;
                case 'h': // type length
                    if( str[i+1] == 'h' ) {
                        length = 'q';
                        i++;
                    } else {
                        length = 'h';
                    }
                    break;
                case 'l':
                    if( str[i+1] == 'l' ) {
                        length = 'b';
                        i++;
                    } else {
                        length = 'l';
                    }
                    break;
                case 'j':
                case 'z':
                case 't':
                case 'L':
                    length = str[i];
                    break;
                case 'u':
                case 'd':
                case 'i':
                case 'F':
                case 'f':
                case 'o':
                case 'p':
                case 'x':
                case 'X':
                case 's':
                case 'c':
                {
                    char *str2 = printf_form_final_str( args, str[i],
                                    length, min_width, precision,
                                    pad_left, use_zeroes, always_sign,
                                    blank_sign, add_punctuation );
                    char *out_copy = concatentate_strings( out, str2 );
                    kfree(out);
                    out = out_copy;
                    
                    pad_left = false;
                    use_zeroes = false;
                    always_sign = false;
                    blank_sign = false;
                    add_punctuation = false;
                    reading_precision = false;
                    reading_width = false;
                    reading_specifier = false;
                    
                    length = 0;
                    min_width = 0;
                    precision = 0;
                    
                    break;
                }
                case 'n':
                {
                    pad_left = false;
                    use_zeroes = false;
                    always_sign = false;
                    blank_sign = false;
                    add_punctuation = false;
                    reading_precision = false;
                    reading_width = false;
                    reading_specifier = false;
                    
                    length = 0;
                    min_width = 0;
                    precision = 0;
                    
                    signed int* ptr = va_arg(args, signed int*);
                    *ptr = strlen(out);
                    break;
                }
                default:
                    pad_left = false;
                    use_zeroes = false;
                    always_sign = false;
                    blank_sign = false;
                    add_punctuation = false;
                    reading_precision = false;
                    reading_width = false;
                    reading_specifier = false;
                    
                    length = 0;
                    min_width = 0;
                    precision = 0;
                    break;
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