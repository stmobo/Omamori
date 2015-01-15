#include "arch/x86/multitask.h"
#include "device/vga.h"
#ifdef ENABLE_SERIAL_LOGGING
#include "device/serial.h"
#endif
#include "lib/sync.h"
#include "arch/x86/debug.h"
#include "core/scheduler.h"
#include <stdarg.h>

const char* numeric_alpha = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static vector<char*> logger_lines_to_write;
static mutex logger_vec_lock;
static process *logger_process = NULL;

size_t strlen(char* str)
{
    if( (uint32_t)str < 0x1000 ) // also catches NULL pointer derefs!
        return 0;
	size_t ret = 0;
	while ( str[ret] != 0 )
		ret++;
	return ret;
}

char* int_to_octal(unsigned long long int n) {
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

char* int_to_hex(unsigned long long int n) {
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

char* int_to_bin(unsigned long long int n) {
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
char* int_to_decimal(unsigned long long int n) {
    unsigned int d = 0;
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
    for(unsigned int i=1;i<d;i++) {
        mem[(d-i)-1] = numeric_alpha[(n2%10)];
        n2 /= 10;
    }
    mem[d-1] = '\0';
    return mem;
}

char *concatentate_strings(char* str1, char *str2) {
    if( (str1 == NULL) && (str2 == NULL) )
        return NULL;
    if( str1 == NULL ) {
        char *out = (char*)kmalloc( strlen(str2)+1 );
        for(size_t i=0;i<=strlen(str2);i++) {
            out[i] = str2[i];
        }
        return out;
    }
    if( str2 == NULL ) {
        char *out = (char*)kmalloc( strlen(str1)+1 );
        for(size_t i=0;i<=strlen(str1);i++) {
            out[i] = str1[i];
        }
        return out;
    }
    int len = (strlen(str1) + strlen(str2));
    char *out = (char*)kmalloc(len + 2);
    for(size_t i=0;i<=strlen(str1);i++) {
        out[i] = str1[i];
        if( out[i] == 0 )
            break;
    }
    for(size_t i=0;i<=strlen(str2);i++) {
        out[ i+strlen(str1) ] = str2[i];
        if( out[ i+strlen(str1) ] == 0 )
            break;
    } 
    out[len] = '\0';
    return out;
}

char *append_char(char* str1, char c) {
    if( str1 == NULL ) {
        char *ret = (char*)kmalloc( 2 );
        ret[0] = c;
        ret[1] = 0;
        return ret;
    }
    char *out = (char*)kmalloc( strlen(str1)+2 );
    for(unsigned int i=0;i<strlen(str1);i++) {
        out[i] = str1[i];
    }
    out[strlen(str1)] = c;
    out[strlen(str1)+1] = 0;
    return out;
}

char *printf_form_final_str( va_list &args, char specifier,
    char type_length, unsigned int min_width, unsigned int precision,
    bool pad_left, bool pad_zeroes, bool add_sign,
    bool add_blank, bool add_identifier ) 
    {
    
    bool is_negative = false;
    char *str2 = NULL;
    switch( specifier ){
        case 'u':
        {
            unsigned long long int n;
            switch( type_length ) {
                case 'q':
                    n = (unsigned char)va_arg(args, int);
                    break;
                case 'h':
                    n = (unsigned short int)va_arg(args, int);
                    break;
                case 'l':
                    n = va_arg(args, unsigned long int);
                    break;
                case 'b':
                    n = va_arg(args, unsigned long long int);
                    break;
                case 'j':
                    n = va_arg(args, uintmax_t);
                    break;
                case 'z':
                    n = va_arg(args, size_t);
                    break;
                case 't':
                    n = va_arg(args, ptrdiff_t);
                    break;
                default:
                    n = va_arg(args, unsigned int);
                    break;
            }
            str2 = int_to_decimal(n);
            break;
        }
        case 'o':
        {
            unsigned long long int n;
            switch( type_length ) {
                case 'q':
                    n = (unsigned char)(va_arg(args, int));
                    break;
                case 'h':
                    n = (unsigned short int)(va_arg(args, int));
                    break;
                case 'l':
                    n = va_arg(args, unsigned long int);
                    break;
                case 'b':
                    n = va_arg(args, unsigned long long int);
                    break;
                case 'j':
                    n = va_arg(args, uintmax_t);
                    break;
                case 'z':
                    n = va_arg(args, size_t);
                    break;
                case 't':
                    n = va_arg(args, ptrdiff_t);
                    break;
                default:
                    n = va_arg(args, unsigned int);
                    break;
            }
            str2 = int_to_octal(n);
            break;
        }
        case 's':
        {
            char *arg_str;
            arg_str = va_arg(args, char*);
            if( (uint32_t)arg_str < 0x1000 ) // dirty hack (nothing's supposed to be down there)
                return NULL;
            if(precision != 0) {
                str2 = (char*)kmalloc(precision+2);
                if(str2 == NULL)
                    break;
                str2[precision+1] = 0;
                for(unsigned int i=0;i<=precision;i++) {
                    str2[i] = arg_str[i];
                    if(str2[i] == 0)
                        break;
                }
            } else {
                str2 = (char*)kmalloc( strlen(arg_str)+2 );
                if(str2 == NULL)
                    break;
                str2[strlen(arg_str)+1] = 0;
                for(unsigned int i=0;i<=strlen(arg_str);i++) {
                    str2[i] = arg_str[i];
                    if(str2[i] == 0)
                        break;
                }
            }
            break;
        }
        case 'X':
        {
            unsigned long long int n;
            switch( type_length ) {
                case 'q':
                    n = (unsigned char)va_arg(args, int);
                    break;
                case 'h':
                    n = (unsigned short int)va_arg(args, int);
                    break;
                case 'l':
                    n = va_arg(args, unsigned long int);
                    break;
                case 'b':
                    n = va_arg(args, unsigned long long int);
                    break;
                case 'j':
                    n = va_arg(args, uintmax_t);
                    break;
                case 'z':
                    n = va_arg(args, size_t);
                    break;
                case 't':
                    n = va_arg(args, ptrdiff_t);
                    break;
                default:
                    n = va_arg(args, unsigned int);
                    break;
            }
            str2 = int_to_hex(n);
        }
        case 'p':
        {
            void *ptr = va_arg(args, void*);
            str2 = int_to_hex( reinterpret_cast<uint32_t>(ptr) );
            break;
        }
        case 'x':
        {
            unsigned long long int n;
            switch( type_length ) {
                case 'q':
                    n = (unsigned char)va_arg(args, int);
                    break;
                case 'h':
                    n = (unsigned short int)va_arg(args, int);
                    break;
                case 'l':
                    n = va_arg(args, unsigned long int);
                    break;
                case 'b':
                    n = va_arg(args, unsigned long long int);
                    break;
                case 'j':
                    n = va_arg(args, uintmax_t);
                    break;
                case 'z':
                    n = va_arg(args, size_t);
                    break;
                case 't':
                    n = va_arg(args, ptrdiff_t);
                    break;
                default:
                    n = va_arg(args, unsigned int);
                    break;
            }
            str2 = int_to_hex(n);
            for(unsigned int i=0;i<strlen(str2);i++) {
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
            unsigned long long int un;
            long long int n;
            switch( type_length ) {
                case 'q':
                    n = va_arg(args, int);
                    break;
                case 'h':
                    n = va_arg(args, int);
                    break;
                case 'l':
                    n = va_arg(args, long int);
                    break;
                case 'b':
                    n = va_arg(args, long long int);
                    break;
                case 'j':
                    n = va_arg(args, intmax_t);
                    break;
                case 'z':
                    n = va_arg(args, size_t);
                    break;
                case 't':
                    n = va_arg(args, ptrdiff_t);
                    break;
                default:
                    n = va_arg(args, int);
                    break;
            }
            if( n < 0 ) {
                is_negative = true;
                n *= -1;
            }
            un = n;
            str2 = int_to_decimal(un);
            break;
        } // case 'd' / 'i'
        default:
            return NULL;
    }
    if( str2 == NULL )
        return NULL;
    if(min_width > (strlen(str2)-1)) {
        int n_padding = (strlen(str2)-1) - min_width;
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
    char *out = NULL;
    
    bool reading_specifier = false;
    
    // Flags
    bool pad_left = false; // if (min_width > strlen(int_to_<whatever>(next_arg)) then pad
    bool use_zeroes = false; // ..for padding (see above)
    bool always_sign = false;
    bool blank_sign = false;
    bool add_punctuation = false; // add '0', '0x', or decimal points depending on output format
    
    char length = 0; // 'q' instead of 'hh' (signed char) and 'b' instead of 'll' (long long)
    
    unsigned int min_width = 0;
    unsigned int precision = 0;
    bool reading_precision = false;
    bool reading_width = false;
    
    for( size_t i=0;i<strlen(const_cast<char*>(str))+1;i++ ) {
        if(str[i] == 0)
            break;
        if( str[i] == '%' ) {
            if( str[i+1] == '%' ) {
                char *out_copy = append_char( out, '%' );
                if( out )
                    kfree(out);
                out = out_copy;
                i++;
            } else {
                reading_specifier = true;
                
                pad_left = false;
                use_zeroes = false;
                always_sign = false;
                blank_sign = false;
                add_punctuation = false;
                reading_precision = false;
                reading_width = false;
                
                length = 0;
                min_width = 0;
                precision = 0;
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
                case 'q':
                case 'b':
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
                {
                    char *str2 = printf_form_final_str( args, str[i],
                                    length, min_width, precision,
                                    pad_left, use_zeroes, always_sign,
                                    blank_sign, add_punctuation );
                    if( str2 == NULL ) {
                        break;
                    }
                    char *out_copy = concatentate_strings( out, str2 );
                    if( out != NULL ) {
                        kfree(out);
                    }
                    out = out_copy;
                    reading_specifier = false;
                    break;
                }
                case 'c':
                {
                    char c = va_arg(args, int);
                    char* out_copy = append_char( out, c );
                    if( out )
                        kfree(out);
                    out = out_copy;
                    reading_specifier = false;
                    break;
                }
                case 'n':
                {
                    signed int* ptr = va_arg(args, signed int*);
                    *ptr = strlen(out);
                    reading_specifier = false;
                    break;
                }
                default:
                    reading_specifier = false;
                    break;
            }
        } else {
            char *out_copy = append_char( out, str[i] );
            if( out )
                kfree(out);
            out = out_copy;
            reading_specifier = false;
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

static mutex __kprintf_lock;
inline void __logger_do_writeout(char* o) {
#ifdef ENABLE_SERIAL_LOGGING
    if( serial_initialized ) {
        serial_write( o );
    }
#endif
    __kprintf_lock.lock();
    terminal_writestring( o );
    __kprintf_lock.unlock();
}

// Print something to the screen, but take a va_list.
void kprintf_varg(const char *str, va_list args) {
    char *o = ksprintf_varg( str, args );
    if( logger_process == NULL ) {
        __logger_do_writeout(o);
        kfree(o);
    } else {
        logger_vec_lock.lock();
        logger_lines_to_write.add_end(o);
        logger_vec_lock.unlock();
        logger_process->state = process_state::runnable;
        process_add_to_runqueue(logger_process);
    }
}

static uint32_t stat = 0;

// Print something to screen.
// don't call this from irq context, it definitely will break stuff
void kprintf(const char *str, ...) {
    va_list args;
    va_start(args, str);
    
    char *o = ksprintf_varg( str, args );
    
    if( logger_process == NULL ) {
        __logger_do_writeout(o);
        kfree(o);
    } else {
        logger_vec_lock.lock();
        logger_lines_to_write.add_end(o);
        logger_vec_lock.unlock();
        __sync_bool_compare_and_swap( &stat, 0, 1 );
        logger_process->state = process_state::runnable;
        process_add_to_runqueue(logger_process);
    }
    
    va_end(args);
}

void logger_process_func() {
    while(true) {
        if(logger_lines_to_write.count() > 0) {
            logger_vec_lock.lock();
            char *o = logger_lines_to_write.remove();
            logger_vec_lock.unlock();
            __logger_do_writeout(o);
            kfree(o);
        } else {
            __sync_bool_compare_and_swap( &stat, 1, 0 );
            process_current->state = process_state::waiting;
            process_switch_immediate();
        }
    }
}

void logger_flush_buffer() {
    while( stat == 1 ) {
        process_switch_immediate();
    }
}

void logger_initialize() {
    logger_process = new process( (size_t)&logger_process_func, false, 0, "kernel_logger", NULL, 0);
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