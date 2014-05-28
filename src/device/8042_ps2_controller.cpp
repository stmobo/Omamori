// ps2_controller.cpp 

#include "includes.h"
#include "arch/x86/irq.h"
#include "arch/x86/pic.h"
#include "core/scheduler.h"
#include "device/vga.h"
#include "device/pit.h"
#include "device/ps2_controller.h"
#include "lib/sync.h"

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

unsigned char port1_input_buffer[256];
unsigned char port2_input_buffer[256];

mutex     irq1_buffer_mutex;
semaphore irq1_buffer_fill(0,256);
semaphore irq1_buffer_empty(256,256);

int port1_buffer_length = 0;
int port2_buffer_length = 0;

unsigned char ps2_receive_byte(bool port2) {
    //kprintf("ps2_receive_byte: checking/waiting for data...\n");
    irq1_buffer_fill.acquire(1);
    irq1_buffer_mutex.lock();
    
    //kprintf("ps2_receive_byte: there's something here!\n");
    // get data
    uint8_t data = port1_input_buffer[--port1_buffer_length];
    if( port1_buffer_length <= 0 )
        port1_buffer_length = 255;
    
    irq1_buffer_mutex.unlock();
    irq1_buffer_empty.release(1);
    //kprintf("ps2_receive_byte: got data 0x%x.\n", (unsigned long long int)data);
    
    return data;
}

unsigned char delayed_handler_data = 0;
void irq1_delayed_handler() {
    while(true) {
#ifdef DEBUG
        kprintf("IRQ 1! Data=0x%x\n", delayed_handler_data);
#endif
        
        irq1_buffer_empty.acquire(1);
        irq1_buffer_mutex.lock();
        
        port1_input_buffer[port1_buffer_length++] = delayed_handler_data;
        if(port1_buffer_length > 255)
            port1_buffer_length = 0;
            
        irq1_buffer_mutex.unlock();
        irq1_buffer_fill.release(1);
        
        /*
        message msg;
        msg.message_name = "ps2_data_port1";
        msg.data = (void*)delayed_handler_data;
        send_message_all( msg );
        */
        process_current->state = process_state::waiting;
        process_switch_immediate();
    }
}

void irq1_handler() {
    delayed_handler_data = io_inb(0x60);
    irq1_handler_process->state = process_state::runnable;
    process_add_to_runqueue( irq1_handler_process ); // schedule the SLIH to run.
}

void irq12_handler() {
    unsigned char data = io_inb(0x60);
#ifdef DEBUG
    kprintf("IRQ 12! Data=0x%x\n", data);
#endif
    port2_input_buffer[port2_buffer_length] = data;
    port2_buffer_length++;
    if(port2_buffer_length > 255)
        port2_buffer_length = 0;
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
    // Prepare the SLIH.
    irq1_handler_process = new process( (size_t)&irq1_delayed_handler, false, 0, "irq1_handler_process",NULL, 0 );
    spawn_process( irq1_handler_process, false ); // don't immediately schedule the process to run
    
    // Now enable interrupts.
    irq_add_handler( 1, (size_t)&irq1_handler );
    irq_add_handler( 12, (size_t)&irq12_handler );
    ps2_send_command(PS2_CMD_WRITE_CCB);
    ps2_send_byte(PS2_CCB_PORT1_INT | PS2_CCB_PORT2_INT | PS2_CCB_SYS_FLAG | PS2_CCB_PORT1_CLK | PS2_CCB_PORT2_CLK, false);
}

uint16_t ps2_get_ident_bytes(bool port2) {
    if(!port2)
        return port1_ident;
    return port2_ident;
}