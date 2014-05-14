// serial.cpp - serial communications
#include "serial.h"
#include "sys.h"
#include "dynmem.h"
#include "pic.h"
#include "irq.h"
#include "vga.h"

char input_buffer[SERIAL_BUFFER_SIZE];
int input_length = 0;

char output_buffer[SERIAL_BUFFER_SIZE];
int output_length = 0;

bool serial_initialized = false;

// set_dlab - set or clear the DLAB bit.
void set_dlab(short base, bool val) {
    char lctrl = io_inb(base+3);
    if(val)
        lctrl |= 0x80;
    else
        lctrl &= 0x7F;
    io_outb(lctrl, base+3);
}

// set_divisor - set serial port speed
void set_divisor(short base, short div) {
    set_dlab(base, true);
    io_outb((char)(div&0xFF), base);
    io_outb((char)((div>>8)&0xFF), base+1);
    set_dlab(base, false);
}

void set_lp_settings(short base, char settings) {
    io_outb(settings, base+3);
}

// identify_uart - identify what UART chip the system has
int identify_uart(short base_port) {
    io_outb(0xE7, base_port+FCR_IIR_OFFSET);
    char iir = io_inb(base_port+FCR_IIR_OFFSET);
    if(iir&(1<<6)) {
        if(iir&(1<<7)) {
            if(iir&(1<<5)) {
                return IDENT_16750;
            } else {
                return IDENT_16550A;
            }
        } else {
            return IDENT_16550;
        }
    } else {
        io_outb(0x77, base_port+SR_OFFSET);
        char returned = io_inb(base_port+SR_OFFSET);
        if(returned == 0x77)
            return IDENT_16450;
        else
            return IDENT_8250;
    }
}

// serial_print_basic - Basic serial text output.
// Blocks execution until completion, only use in scenarios where the serial queue hasn't been set up yet or can't be
//   used, such as before the PICs are set up. 
void serial_print_basic(char* data) {
    size_t len = strlen(data);
    for(unsigned int i=0;i<len;i++) {
        serial_transmit(COM1_BASE_PORT, data[i]);
    }
}

void serial_enable_interrupts() {
    io_outb(COM1_BASE_PORT+IER_OFFSET, 3);
}

void serial_disable_interrupts() {
    io_outb(COM1_BASE_PORT+IER_OFFSET, 0);
}


// serial_transmit - Transmit a single byte.
void serial_transmit(short base, char out) {
    while((io_inb(base+LSR_OFFSET) & LSR_EMPTY_TRANS_HOLD) == 0);
    
    io_outb(base, out);
}

// serial_receive - Receive a single byte.
char serial_receive(short base) {
    while((io_inb(base+LSR_OFFSET) & LSR_DATA_READY) == 0);
    
    return io_inb(base);
}

int read_uart_fifo(short base) {
    int bytes_read = 0;
    while( io_inb(base+LSR_OFFSET) & LSR_DATA_READY ) {
        input_buffer[input_length] = io_inb(base);
        input_length += 1;
        input_length %= SERIAL_BUFFER_SIZE;
        bytes_read++;
    }
    return bytes_read;
}

int write_uart_fifo(short base) {
    int bytes_written = 0;
    char hex[8];
    if(output_length == 0)
        return 0;
    serial_disable_interrupts();
    while((io_inb(base+LSR_OFFSET) & LSR_EMPTY_TRANS_HOLD)) {
        if(bytes_written > output_length)
            break;
        io_outb(base, output_buffer[bytes_written]);
        bytes_written++;
    }
    for(int i=0;i<output_length-bytes_written;i++) {
        output_buffer[i] = output_buffer[i+bytes_written];
    }
    output_length -= bytes_written;
    serial_enable_interrupts();
    return bytes_written;
}

// serial_irq - UART interrupt handler
void serial_irq() {
    short port = COM1_BASE_PORT;
    char iir;
    if( ((iir = io_inb(COM1_BASE_PORT+2)) & (IIR_INT_NOT_PENDING)) == 0 ) {
        port = COM1_BASE_PORT;
    } else if( ((iir = io_inb(COM2_BASE_PORT+2)) & (IIR_INT_NOT_PENDING)) == 0 ) {
        port = COM2_BASE_PORT;
    } else if( ((iir = io_inb(COM3_BASE_PORT+2)) & (IIR_INT_NOT_PENDING)) == 0 ) {
        port = COM3_BASE_PORT;
    } else if( ((iir = io_inb(COM4_BASE_PORT+2)) & (IIR_INT_NOT_PENDING)) == 0 ) {
        port = COM4_BASE_PORT;
    }
    
    if(iir & (IIR_INT_RECV_DATA_AVAIL))
        read_uart_fifo(port);
    if(iir & (IIR_INT_THR_EMPTY))
        write_uart_fifo(port);
}

char* serial_read(int *ret_bytes) {
    if(input_length == 0) {
        while( (io_inb(COM1_BASE_PORT+LSR_OFFSET) & LSR_DATA_READY) == 0 )
            wait_for_interrupt();
    }
    
    serial_disable_interrupts();
    
    char* ret = kmalloc(input_length);
    *ret_bytes = input_length;
    int old_i_len = input_length;
    int ctr = input_length;
    for(int i=0;i<old_i_len;i++) {
        ret[i] = input_buffer[i];
        input_buffer[i] = 0;
        ctr--;
    }
    input_length = ctr;
    
    serial_enable_interrupts();
    return ret;
}

void serial_write(char* data) {
    char hex[8];
    int len = strlen(data);
    //terminal_writestring("Entering serial_write.\n");
    while((output_length+len) > SERIAL_BUFFER_SIZE)
        wait_for_interrupt();
    /*
    while( (io_inb(COM1_BASE_PORT+LSR_OFFSET) & LSR_EMPTY_TRANS_HOLD) == 0 )
        wait_for_interrupt();
    */
    
    serial_disable_interrupts();
    
    int offset = output_length;
    for(int i=0;i<len;i++) {
        output_buffer[offset] = data[i];
        offset++;
    }
    output_length = offset;
    
    serial_enable_interrupts();
    //write_uart_fifo(COM1_BASE_PORT);
}

// initialize_serial - Set up a serial port.
void initialize_serial(short base, short divisor) {
    io_outb(base+IER_OFFSET, 0);
    set_divisor(base, divisor);
    io_outb(base+LCR_OFFSET, LP_8N1);
    io_outb(base+FCR_IIR_OFFSET, FCR_FIFO_ON | FCR_FIFO_CLEAR_RECV | FCR_FIFO_CLEAR_TRANS | FCR_FIFO_INT_TRIG_14x);
    io_outb(base+MCR_OFFSET, MCR_DATA_TERM_READY | MCR_REQUEST_TO_SEND | MCR_AUX_OUT_2);
    add_irq_handler(4, (size_t)&serial_irq);
    serial_enable_interrupts();
    serial_initialized = true;
}

void flush_serial_buffer(void* n) {
    while(output_length > 0) {
        wait_for_interrupt();
    }
#ifdef DEBUG
    terminal_writestring("Serial buffers flushed.\n");
#endif
}
