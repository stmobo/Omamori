// ps2_controller.cpp 

#include "includes.h"
#include "vga.h"
#include "pit.h"
#include "pic.h"
#include "irq.h"
#include "ps2_controller.h"

unsigned char port1_input_buffer[256];
unsigned char port2_input_buffer[256];

// Points to the next EMPTY area (port1_input_buffer[port1_buffer_length] == <undefined>!)
int port1_buffer_length = 0;
int port2_buffer_length = 0;

bool port1_status = false;
bool port2_status = false;

short port1_ident = 0;
short port2_ident = 0;

// Screw the PS2 controller. Seriously.
// Why don't you support interrupt-driven sending?!
// I had to implement half a dozen things because you can't support interrupt-driven sending!
// (oh well, I had to implement them at some point anyways)

bool ps2_send_byte(unsigned char data, bool port2) {
    if(port2) {
        io_outb(0x64, PS2_CMD_WRITE_PORT2); // write to port 2
        io_wait();
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

bool ps2_send_command(unsigned char cmd) {
    unsigned long long int start_time = get_sys_time_counter();
    while( get_sys_time_counter() < start_time+PS2_RESP_TIMEOUT ) {
        unsigned char stat_reg = io_inb(0x64);
        if(stat_reg & ~(PS2_STA_INPUT_FULL)) {
            io_outb(0x64, cmd);
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

unsigned char ps2_receive_byte(bool port2) {
    if(!port2) {
        while(port1_buffer_length <= 0)
            wait_for_interrupt();
        unsigned char byte = port1_input_buffer[0];
        for(int i=0;i<port1_buffer_length;i++) {
            port1_input_buffer[i] = port1_input_buffer[i+1];
        }
        port1_input_buffer[port1_buffer_length-1] = 0;
        port1_buffer_length--;
        return byte;
    } else {
        while(port2_buffer_length <= 0)
            wait_for_interrupt();
        unsigned char byte = port2_input_buffer[0];
        for(int i=0;i<port2_buffer_length;i++) {
            port2_input_buffer[i] = port2_input_buffer[i+1];
        }
        port2_input_buffer[port2_buffer_length-1] = 0;
        port2_buffer_length--;
        return byte;
    }
}

void irq1_handler() {
    terminal_writestring("IRQ 1!");
    unsigned char data = io_inb(0x60);
    port1_input_buffer[port1_buffer_length] = data;
    port1_buffer_length++;
    if(port1_buffer_length > 255)
        port1_buffer_length = 0;
}

void irq12_handler() {
    terminal_writestring("IRQ 12!");
    unsigned char data = io_inb(0x60);
    port2_input_buffer[port2_buffer_length] = data;
    port2_buffer_length++;
    if(port2_buffer_length > 255)
        port2_buffer_length = 0;
}

void ps2_set_interrupt_status(bool status, bool port2) {
    if(!port2)
        irq_mask(1, !status);
    else
        irq_mask(12, !status);
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
    
    // Get PS2 port self-test status
    ps2_send_command(PS2_CMD_TEST_PORT1);
    if(ps2_wait_for_input() != 0)
        port1_status = false;
        
    ps2_send_command(PS2_CMD_TEST_PORT2);
    if(ps2_wait_for_input() != 0)
        port2_status = false;
    
    // Enable ports
    
    if(port1_status)
        ps2_send_command(PS2_CMD_ENABLE_PORT1);
    if(port2_status)
        ps2_send_command(PS2_CMD_ENABLE_PORT2);
    
    // Reset all devices and get their status
    if(port1_status) {
        ps2_send_byte(0xFF, false);
        if(ps2_wait_for_input() != PS2_RESP_SUCCESS)
            port1_status = false;
    }
    if(port2_status) {
        ps2_send_byte(0xFF, true);
        if(ps2_wait_for_input() != PS2_RESP_SUCCESS)
            port2_status = false;
    }
   
    // Get device identification bytes
    unsigned long long int start_time = get_sys_time_counter();
    bool receiving_second_byte = false;
    while( get_sys_time_counter() < start_time+PS2_RESP_TIMEOUT ) {
        unsigned char ident_byte = ps2_wait_for_input();
        port2_ident <<= 8;
        port2_ident |= ident_byte;
    }
    
    start_time = get_sys_time_counter();
    receiving_second_byte = false;
    while( get_sys_time_counter() < start_time+PS2_RESP_TIMEOUT ) {
        unsigned char ident_byte = ps2_wait_for_input();
        port2_ident <<= 8;
        port2_ident |= ident_byte;
    }
    
    // Now enable interrupts.
    add_irq_handler( 1, (size_t)&irq1_handler );
    add_irq_handler( 12, (size_t)&irq12_handler );
    ps2_send_command(PS2_CMD_WRITE_CCB);
    ps2_send_byte(PS2_CCB_PORT1_INT | PS2_CCB_PORT2_INT | PS2_CCB_SYS_FLAG | PS2_CCB_PORT1_CLK | PS2_CCB_PORT2_CLK, false);
}

short ps2_get_ident_bytes(bool port2) {
    if(!port2)
        return port1_ident;
    return port2_ident;
}