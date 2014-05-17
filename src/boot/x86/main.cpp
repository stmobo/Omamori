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
#include "pci.h"
#include "ps2_controller.h"
#include "ps2_keyboard.h"
#include "multitask.h"

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
    gdt_init();
    idt_init();
    k_heap_init(HEAP_START_ADDR);
    initialize_pageframes(mb_info);
    
    //system_halt;
    kprintf("Kernel begins at physical address 0x%x, corresponding to pageframe ID %u.\n", (unsigned long long int)(&kernel_start_phys), (unsigned long long int)(pageframe_get_block_from_addr( (size_t)&kernel_start_phys )) );
    kprintf("Kernel ends at physical address 0x%x, corresponding to pageframe ID %u.\n", (unsigned long long int)(&kernel_end_phys), (unsigned long long int)(pageframe_get_block_from_addr( (size_t)&kernel_end_phys )) );
    
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
    
    terminal_writestring("Test: ");
    terminal_writestring(int_to_decimal(test2));
    kprintf("\nTest 2: %u / 0x%x\n.", (unsigned long long int)test2, (unsigned long long int)test2);
    
    atexit(&flush_serial_buffer, NULL);
    
    //block_for_interrupt(1);
    kprintf("Time since system startup: %u ticks.", (unsigned long long int)get_sys_time_counter());
    
    kprintf("Initializing PS/2 controller.\n");
    ps2_controller_init();
    
    kprintf("Initializing PS/2 keyboard.\n");
    ps2_keyboard_initialize();
    
    kprintf("Allocating 160 4k frames (636k memory).\n");
    framez = pageframe_allocate(160);
    kprintf("Frame allocation 1 starts at ID %u\n", framez->id);
    kprintf("Allocating 35 4k frames (140k memory).\n");
    framez2 = pageframe_allocate(35);
    kprintf("Frame allocation 2 starts at ID %u\n", framez2->id);
    kprintf("Deallocating all frames.\n");
    pageframe_deallocate(framez2, 35);
    pageframe_deallocate(framez, 160);
    kprintf("Allocating 2x13 4k frames (52k memory).\n");
    framez = pageframe_allocate(13);
    framez2 = pageframe_allocate(13);
    kprintf("Frame allocation 1 starts at ID %u\n", framez->id);
    kprintf("Frame allocation 2 starts at ID %u\n", framez2->id);
    pageframe_deallocate(framez2, 13);
    pageframe_deallocate(framez, 13);
    
    kprintf("Testing kernel vmem allocation.\n");
    size_t vmem_test = k_vmem_alloc( 5 );
    kprintf("Virtual allocation starts at address 0x%x.\n", (unsigned long long int)vmem_test);
    
    kprintf("Initializing PCI.\n");
    pci_check_bus(0);
    
    kprintf("Initiating page fault!\n");
    int *pf_test = (int*)(0xC0400000);
    *pf_test = 5;
    kprintf("Memory read-back test: %u (should be 5)\n", (unsigned long long int)*pf_test);
    
    kprintf("Initializing usermode.\n");
    initialize_userspace();
    
    kprintf("Press ENTER to continue...\n");
    terminal_putchar('>');
    while(true) {
        int len;
        char *line = ps2_keyboard_readline(&len);
        if(strcmp(line, "exit")) {
            terminal_putchar('\n');
            kfree(line);
            //flush_serial_buffer(NULL);
            break;
        } else if(strcmp(line, "time")) {
            kprintf("Time since system startup: %u ticks.", (unsigned long long int)get_sys_time_counter());
        } else {
            kprintf(line, len);
        }
        kfree(line);
        terminal_writestring("\n>");
    }
    
    kprintf("Setup complete, halting!\n");
    unsigned long long int last_ticked = 0;
    //timer t(1000, true, true, NULL);
    while(true) {
        //if(t.get_active()) {
        //    kprintf("Tock.");
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