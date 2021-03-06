// serial.cpp - serial communications
#include "includes.h"
#include "arch/x86/sys.h"
#include "arch/x86/irq.h"
#include "core/scheduler.h"
#include "core/message.h"
#include "device/serial.h"
#include "lib/refcount.h"

static process *uart_writer_process; 
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

/*
unsigned char* read_uart( short base, size_t* len ) {
	unsigned int bytes_read = 0;
	vector<unsigned char> buf;
	while( io_inb(base+LSR_OFFSET) & LSR_DATA_READY ) {
		buf.add_end( io_inb(base) );
	}
	unsigned char* b2 = kmalloc(buf.count());
	*len = buf.count();

	for(unsigned int i=0;i<buf.count();i++) {
		b2[i] = buf[i];
	}

	return b2;
}


unsigned int read_uart_fifo(short base) {
    unsigned int bytes_read = 0;
    while( io_inb(base+LSR_OFFSET) & LSR_DATA_READY ) {
        input_buffer[input_buffer_head++] = io_inb(base);
        input_buffer_head %= SERIAL_BUFFER_SIZE;
        bytes_read++;
    }
    if( uart_read_wait_process != NULL ) {
        uart_read_wait_process->state = process_state::runnable;
        process_add_to_runqueue(uart_read_wait_process);
    }
    return bytes_read;
}


unsigned int write_uart_fifo(short base) {
    unsigned int bytes_written = 0;
    while( (output_buffer_head != output_buffer_tail) && (io_inb(base+LSR_OFFSET) & LSR_EMPTY_TRANS_HOLD) ) {
        io_outb(base, output_buffer[output_buffer_tail++]);
        output_buffer_tail %= SERIAL_BUFFER_SIZE;
        bytes_written++;
    }
    return bytes_written;
}

char* serial_read(int *ret_bytes) {
    vector<char> ret;
    input_buffer_readers_mutex.lock();
    uart_read_wait_process = process_current;
    while( input_buffer_head != input_buffer_tail ) {
        char d = input_buffer[input_buffer_tail++];
        input_buffer_tail %= SERIAL_BUFFER_SIZE;
        ret.add_end(d);
    }
    uart_read_wait_process = NULL;
    input_buffer_readers_mutex.unlock();
    char *ret2 = (char*)kmalloc(ret.length()+1);
    for(unsigned int i=0;i<ret.length();i++) {
        ret2[i] = ret[i];
    }
    ret2[ ret.length() ] = 0;
    if( ret_bytes != NULL )
        *ret_bytes = ret.length();
    return ret2;
}

unsigned int write_uart_fifo(short base) {
    unsigned int bytes_written = 0;
    while( (output_buffer_head != output_buffer_tail) && (io_inb(base+LSR_OFFSET) & LSR_EMPTY_TRANS_HOLD) ) {
        io_outb(base, output_buffer[output_buffer_tail++]);
        output_buffer_tail %= SERIAL_BUFFER_SIZE;
        bytes_written++;
    }
    return bytes_written;
}

void serial_write(char* data) {
    unsigned int len = strlen(data);
    output_buffer_writers_mutex.lock();
    
    // might seem a bit redundant, but in the off chance that the empty semaphore is empty but the writer process' asleep...
    // we want to make sure that the writer process eventually frees up some space
    if( (uart_writer_process != NULL) && (uart_writer_process->state != process_state::runnable) ) {
        uart_writer_process->state = process_state::runnable;
        process_add_to_runqueue(uart_writer_process);
    }
    
    for(unsigned int i=0;i<len;i++) {
        while( output_buffer_head == output_buffer_tail-1 ) { // avoid going in a full circle (spin until it works)
            if( (uart_writer_process != NULL) && (uart_writer_process->state != process_state::runnable) ) {
                uart_writer_process->state = process_state::runnable;
                process_add_to_runqueue(uart_writer_process);
            }
            process_switch_immediate();
        }
        output_buffer[output_buffer_head++] = data[i];
        output_buffer_head %= SERIAL_BUFFER_SIZE;
        
        // wake up the writer process
        if( (uart_writer_process != NULL) && (uart_writer_process->state != process_state::runnable) ) {
            uart_writer_process->state = process_state::runnable;
            process_add_to_runqueue(uart_writer_process);
        }
    }
    output_buffer_writers_mutex.unlock();
}

// bottom half processes
/*
void uart_fifo_reader() {
    while(true) {
        if( read_uart_fifo(COM1_BASE_PORT) == 0 ) {
            process_current->state = process_state::waiting;
            process_switch_immediate();
        }
    }
}
*/

void uart_writer() {
	channel_receiver ch = listen_to_channel("serial_xmit");
    while(true) {
    	ch.wait();

    	unique_ptr<message> m = ch.queue.remove(0);

    	unsigned short base;
    	unsigned char* data = (unsigned char*)m->data;

    	if( (io_inb(COM1_BASE_PORT+LSR_OFFSET) & LSR_EMPTY_TRANS_HOLD) == 0 ) {
    		process_sleep();
    	}

    	for(unsigned int i=0;i<m->data_size;i++) {
    		io_outb(COM1_BASE_PORT, data[i]);
    	}
    }
}

// serial_irq - UART interrupt handler
bool serial_irq( uint8_t irq_num ) {
    unsigned short port = COM1_BASE_PORT;
    unsigned int port_int = 0;
    char iir;
    if( ((iir = io_inb(COM1_BASE_PORT+2)) & (IIR_INT_NOT_PENDING)) == 0 ) {
        port = COM1_BASE_PORT;
        port_int = 1;
    } else if( ((iir = io_inb(COM2_BASE_PORT+2)) & (IIR_INT_NOT_PENDING)) == 0 ) {
        port = COM2_BASE_PORT;
        port_int = 2;
    } else if( ((iir = io_inb(COM3_BASE_PORT+2)) & (IIR_INT_NOT_PENDING)) == 0 ) {
        port = COM3_BASE_PORT;
        port_int = 3;
    } else if( ((iir = io_inb(COM4_BASE_PORT+2)) & (IIR_INT_NOT_PENDING)) == 0 ) {
        port = COM4_BASE_PORT;
        port_int = 4;
    }

    if( (iir & 7) == 1 ) { // Transmitter holding buffer empty
        process_wake(uart_writer_process);
    } else if( (iir & 7) == 2 ) { // Received data
    	while( io_inb(port+LSR_OFFSET) & LSR_DATA_READY ) {
			unsigned char data = io_inb(port);
			serial_data* d = new serial_data;
			d->data = data;
			d->port = port_int;

			message m( (void*)d, sizeof(serial_data) );
			send_to_channel( "serial_recv", m );
		}
    }
    
    return true;
}

// initialize_serial - Set up a serial port.
void initialize_serial(short base, short divisor) {
    io_outb(base+IER_OFFSET, 0);
    set_divisor(base, divisor);
    io_outb(base+LCR_OFFSET, LP_8N1);
    io_outb(base+FCR_IIR_OFFSET, FCR_FIFO_ON | FCR_FIFO_CLEAR_RECV | FCR_FIFO_CLEAR_TRANS | FCR_FIFO_INT_TRIG_14x);
    io_outb(base+MCR_OFFSET, MCR_DATA_TERM_READY | MCR_REQUEST_TO_SEND | MCR_AUX_OUT_2);
    
    uart_writer_process = new process( (size_t)&uart_writer, false, 0, "uart_writer", NULL, 0 );
    register_channel( "serial_recv" );
    register_channel( "serial_xmit" );
	spawn_process(uart_writer_process);
    irq_add_handler(4, &serial_irq);
    serial_enable_interrupts();
    serial_initialized = true;
}

void initialize_serial() {
    return initialize_serial( COM1_BASE_PORT, LP_8N1 );
}
