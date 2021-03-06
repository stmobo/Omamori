// ps2_controller.cpp 

#include "includes.h"
#include "arch/x86/sys.h"
#include "arch/x86/irq.h"
#include "arch/x86/pic.h"
#include "core/scheduler.h"
#include "device/vga.h"
#include "device/pit.h"
#include "device/ps2_controller.h"
#include "lib/sync.h"
#include "core/message.h"
#include "core/device_manager.h"

bool port1_status = false;
bool port2_status = false;

uint16_t port1_ident = 0;
uint16_t port2_ident = 0;

process *irq1_handler_process = NULL;

// Screw the PS2 controller. Seriously.
// Why don't you support interrupt-driven sending?!
// I had to implement half a dozen things because you can't support interrupt-driven sending!
// (oh well, I had to implement them at some point anyways)

bool ps2_send_command(unsigned char cmd) {
    unsigned long long int start_time = get_sys_time_counter();
    while( get_sys_time_counter() < start_time+PS2_RESP_TIMEOUT ) {
        unsigned char stat_reg = io_inb(0x64);
        if(stat_reg & ~(PS2_STA_INPUT_FULL)) {
            io_outb(0x64, cmd);
            io_wait();
            return true;
        }
    }
    return false;
}

bool ps2_send_byte(unsigned char data, bool port2) {
    if(port2) {
        ps2_send_command(PS2_CMD_WRITE_PORT2);
    }
    unsigned long long int start_time = get_sys_time_counter();
    while( get_sys_time_counter() < start_time+PS2_RESP_TIMEOUT ) {
        unsigned char stat_reg = io_inb(0x64);
        if(stat_reg & ~(PS2_STA_INPUT_FULL)) {
            io_outb(0x60, data);
            return true;
        }
    }
    return false;
}

unsigned char ps2_wait_for_input() {
    unsigned char data = 0xFF;
    unsigned long long int start_time = get_sys_time_counter();
    while( get_sys_time_counter() < start_time+PS2_RESP_TIMEOUT ) {
        unsigned char stat_reg = io_inb(0x64);
        if(stat_reg & ~(PS2_STA_OUTPUT_FULL)) {
            data = io_inb(0x60);
            break;
        }
    }
    return data;
}

unsigned char port2_input_buffer[256];
mutex irq1_buffer_mutex;

int port1_buffer_length = 0;
int port2_buffer_length = 0;

// there's probably a better way to implement this, I just haven't thought of it
unsigned char port1_input_buffer[0x1000];
volatile unsigned int  port1_data_head = 0;
volatile unsigned int  port1_data_tail = 0;
process *current_waiter = NULL;

unsigned char ps2_receive_byte(bool port2) {
    //kprintf("ps2_receive_byte: checking/waiting for data...\n");
    
    /*
    unsigned int data = (unsigned int)msg->data;
    delete msg;
    return (unsigned char)data;
    */

	channel_receiver ch = listen_to_channel("ps2_data");
	while( true ) {
		ch.wait();

		message* m = ch.queue.remove(0);
		if( m != NULL ) {
			ps2_data* d = (ps2_data*)m->data;
			if( d->port == port2 ) {
				unsigned char ret = d->data;
				delete m;
				return ret;
			}
			delete m;
		}
	}

	/*
    irq1_buffer_mutex.lock();
    current_waiter = process_current;
    while( true ) {
        process_current->state = process_state::waiting;
        if( port1_data_tail != port1_data_head ) {
            break;
        }
        process_switch_immediate();
    }
    process_current->state = process_state::runnable;
    uint8_t data = port1_input_buffer[port1_data_tail++];
    port1_data_tail %= 0x1000;
    current_waiter = NULL;
    irq1_buffer_mutex.unlock();
    */
    
    //kprintf("ps2_receive_byte: got data 0x%x.\n", (unsigned long long int)data);
}

bool ps2_irq_handler( uint8_t irq_num ) {
	unsigned char data = io_inb(0x60);

	ps2_data *d = new ps2_data;
	d->data = data;
	d->port = ( irq_num == 12 );

	message m( (void*)d, sizeof(ps2_data) );
	send_to_channel("ps2_data", m);

	return ((irq_num == 1) || (irq_num == 12));
}

void ps2_set_interrupt_status(bool status, bool port2) {
    if(!port2)
        irq_set_mask(1, !status);
    else
        irq_set_mask(12, !status);
}

void ps2_controller_init() {
    // disable the ps2 ports first
    ps2_send_command(PS2_CMD_DISABLE_PORT1);
    ps2_send_command(PS2_CMD_DISABLE_PORT2);
    
    // clear the controller's input buffer
    io_inb(0x60);
    
    // Set the CCB
    ps2_send_command(PS2_CMD_WRITE_CCB);
    ps2_send_byte(PS2_CCB_SYS_FLAG | PS2_CCB_PORT1_CLK | PS2_CCB_PORT2_CLK, false);
    
    // Get PS2 controller self-test status
    ps2_send_command(PS2_CMD_TEST_CONTROLLER);
    if(ps2_wait_for_input() != 0x55)
        return; // maybe warn?
#ifdef DEBUG
    kprintf("PS/2 controller self-test passed.\n");
#endif    

    // Get PS2 port self-test statuses
    if(port1_status) {
        ps2_send_command(PS2_CMD_TEST_PORT1);
        unsigned char test_result = ps2_wait_for_input();
        if(test_result != PS2_RESP_SUCCESS) {
#ifdef DEBUG
            kprintf("PS/2 port 1 self-test FAILED, status=0x%x\n", test_result);
#endif
        } else {
            port1_status = true;
#ifdef DEBUG
            kprintf("PS/2 port 1 self-test passed.\n");
#endif
        }
    }
        
    if(port2_status) {
        ps2_send_command(PS2_CMD_TEST_PORT2);
        unsigned char test_result = ps2_wait_for_input();
        if(test_result != PS2_RESP_SUCCESS) {
#ifdef DEBUG
            kprintf("PS/2 port 2 self-test FAILED, status=0x%x\n", test_result);
#endif
            port2_status = false;
        } else {
            port2_status = true;
#ifdef DEBUG
            kprintf("PS/2 port 2 self-test passed.\n");
#endif
        }
    }
    
    // Enable ports
    
    if(port1_status)
        ps2_send_command(PS2_CMD_ENABLE_PORT1);
    if(port2_status)
        ps2_send_command(PS2_CMD_ENABLE_PORT2);
    
    // Reset all devices and get their status
    if(port1_status) {
        ps2_send_byte(0xFF, false);
        unsigned char response = ps2_wait_for_input();
        if((response != PS2_RESP_SUCCESS) && (response != 0xAA)) {
#ifdef DEBUG
            kprintf("PS/2 port 1 reset FAILED, status=0x%x\n", response);
#endif
            port1_status = false;
        } else {
#ifdef DEBUG
            kprintf("PS/2 port 1 reset was successful.\n");
#endif
            port1_status = true;
        }
    }
    
    if(port2_status) {
        ps2_send_byte(0xFF, true);
        unsigned char response = ps2_wait_for_input();
        if((response != PS2_RESP_SUCCESS) && (response != 0xAA)) {
#ifdef DEBUG
            kprintf("PS/2 port 2 reset FAILED, status=0x%x\n", response);
#endif
            port2_status = false;
        } else {
#ifdef DEBUG
            kprintf("PS/2 port 2 reset was successful.\n");
#endif
            port2_status = true;
        }
    }
   
    // Get device identification bytes
    unsigned long long int start_time = get_sys_time_counter();
    if(port1_status) {
        ps2_send_byte(0xF2, false);
        ps2_wait_for_input();
        while( get_sys_time_counter() < start_time+PS2_RESP_TIMEOUT ) {
            unsigned char ident_byte = ps2_wait_for_input();
            port1_ident <<= 8;
            port1_ident |= ident_byte;
        }
    }
    
    if(port2_status) {
        start_time = get_sys_time_counter();
        ps2_send_byte(0xF2, true);
        ps2_wait_for_input();
        while( get_sys_time_counter() < start_time+PS2_RESP_TIMEOUT ) {
            unsigned char ident_byte = ps2_wait_for_input();
            port1_ident <<= 8;
            port1_ident |= ident_byte;
        }
    }
#ifdef DEBUG
    kprintf("Port 1 ident: 0x%x\n", port1_ident);
    kprintf("Port 2 ident: 0x%x\n", port2_ident);
#endif 
    // Now enable interrupts.
    irq_add_handler( 1, &ps2_irq_handler );
    irq_add_handler( 12, &ps2_irq_handler );
    ps2_send_command(PS2_CMD_WRITE_CCB);
    ps2_send_byte(PS2_CCB_PORT1_INT | PS2_CCB_PORT2_INT | PS2_CCB_SYS_FLAG | PS2_CCB_PORT1_CLK | PS2_CCB_PORT2_CLK, false);

    device_manager::device_node* dev = new device_manager::device_node;
	dev->child_id = device_manager::root.children.count();
	dev->enabled = true;
	dev->type = device_manager::dev_type::ps2_controller;
	dev->human_name = const_cast<char*>("8042 Keyboard Controller");

	device_manager::device_resource* res = new device_manager::device_resource;
	res->consumes = true;
	res->type = device_manager::res_type::io_port;
	res->io_port.start = 0x60;
	res->io_port.end = 0x60;

	dev->resources.add_end(res);

	res = new device_manager::device_resource;
	res->consumes = true;
	res->type = device_manager::res_type::io_port;
	res->io_port.start = 0x64;
	res->io_port.end = 0x64;

	dev->resources.add_end(res);

	device_manager::root.children.add_end( dev );

	register_channel("ps2_data");
}

uint16_t ps2_get_ident_bytes(bool port2) {
    if(!port2)
        return port1_ident;
    return port2_ident;
}
