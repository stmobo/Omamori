#include "includes.h"
#include "vga.h"
#include "table.h"
#include "paging.h"
#include "multiboot.h"
#include "dynmem.h"
#include "sys.h"
#include "pic.h"
#include "pit.h"
#include "irq.h"
#include "serial.h"
#include "ps2_controller.h"
#include "ps2_keyboard.h"

bool a = true;

#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif
 
#if !defined(__i386__)
#error "This needs to be compiled with a ix86-elf compiler"
#endif

void test_func(void* n) {
    terminal_writestring("atexit() works.\n");
}

void test_func2() {
    terminal_writestring("Tock.\n");
    write_uart_fifo(COM1_BASE_PORT);
}

void test_func3() {
    terminal_writestring("Tick.\n");
}

bool strcmp(char* s1, char* s2) {
    int i = 0;
    while(true) {
        if(s1[i] != s2[i])
            return false;
        if(s1[i] == 0)
            return true;
        if(s2[i] == 0)
            return true;
        i++;
    }
}

#if defined(__cplusplus)
extern "C"
#endif
void kernel_main(multiboot_info_t* mb_info, unsigned int magic)
{
    char *test;
    int test2 = 0x12345678;
    page_frame* framez;
    page_frame* framez2;
    terminal_initialize();
    terminal_writestring("Project Omamori now starting...\n");
    k_heap_init(HEAP_START_ADDR);
    gdt_init();
    idt_init();
    initialize_pageframes(mb_info);
    //system_halt;
    kprintf("Kernel ends at address 0x%x, corresponding to pageframe ID %u.\n", ((size_t)&kernel_end), (pageframe_get_block_from_addr( (size_t)&kernel_end )) );
    
    terminal_writestring("\nInitializing PICs.\n");
    pic_initialize(PIC_IRQ_OFFSET_1, PIC_IRQ_OFFSET_2);
    set_all_irq_status(true);
    
    terminal_writestring("Initializing PIT.\n");
    pit_initialize(PIT_DEFAULT_FREQ_DIVISOR);
    
    terminal_writestring("Initializing serial interface.\n");
    initialize_serial(COM1_BASE_PORT, 3);
    /*
    serial_print_basic("Serial port 1 active.\n");
    serial_write("Testing serial port behavior.\n");
    serial_write("Testing serial port behavior again\n");
    serial_write("Testing serial port behavior yet again\n");
    */
    
    terminal_writestring("Test:");
    terminal_writestring(int_to_decimal(test2));
    
    atexit(&flush_serial_buffer, NULL);
    
    //block_for_interrupt(1);
    unsigned long long int sys_time = get_sys_time_counter();
    terminal_writestring("\nTime since system startup:");
    terminal_writestring(int_to_decimal(sys_time));
    terminal_writestring(" ms.\n");
    
    terminal_writestring("Initializing PS/2 controller.\n");
    ps2_controller_init();
    
    terminal_writestring("Initializing PS/2 keyboard.\n");
    ps2_keyboard_initialize();
    
    terminal_writestring("Allocating 160 4k frames (636k memory).\n");
    framez = pageframe_allocate(160);
    terminal_writestring("Allocating 35 4k frames (140k memory).\n");
    framez2 = pageframe_allocate(35);
    pageframe_deallocate(framez2, 35);
    pageframe_deallocate(framez, 160);
    framez = pageframe_allocate(13);
    pageframe_deallocate(framez, 13);
    
    terminal_writestring("Press ENTER to continue...\n");
    terminal_putchar('>');
    while(true) {
        int len;
        char *line = ps2_keyboard_readline(&len);
        terminal_writestring(line, len);
        if(strcmp(line, "exit")) {
            terminal_putchar('\n');
            kfree(line);
            break;
        }
        kfree(line);
        terminal_writestring("\n>");
    }
    
    terminal_writestring("Setup complete, halting!\n");
    unsigned long long int last_ticked = 0;
    //timer t(1000, true, true, NULL);
    while(true) {
        //if(t.get_active()) {
        //    terminal_writestring("Tock.");
        //    t.reload();
        //}
        if(last_ticked+1000 <= get_sys_time_counter()) {
            terminal_writestring("Tick.");
            last_ticked = get_sys_time_counter();
        }
    }
    //asm volatile("int $0" : : : "memory");
    //while(true)
    //    asm volatile( "hlt\n\t" : : : "memory");
}