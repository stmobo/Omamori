#include "includes.h"
#include "arch/x86/irq.h"
#include "arch/x86/pic.h"
#include "arch/x86/table.h"
#include "arch/x86/multitask.h"
#include "core/acpi.h"
#include "boot/multiboot.h"
#include "core/paging.h"
#include "core/scheduler.h"
#include "device/pit.h"
#include "device/serial.h"
#include "device/pci.h"
#include "device/ps2_controller.h"
#include "device/ps2_keyboard.h"
#include "device/vga.h"
extern "C" {
    #include "lua.h"
    #include "lualib.h"
    #include "lauxlib.h"
}

#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif
 
#if !defined(__i386__)
#error "This needs to be compiled with a ix86-elf compiler"
#endif

#if !defined(__cplusplus)
#error "This is C++ code, use a C++ compiler."
#endif

void test_func(void* n) {
    terminal_writestring("atexit() works.\n");
}

static int lua_writeout_proxy(lua_State *st) {
    int n_args = lua_gettop(st);
    unsigned int ret = 0;
    for(int i=1;i<=n_args;i++) {
        if( lua_isstring(st, i) ) {
            const char *out = lua_tostring(st, i);
            terminal_writestring(const_cast<char*>(out));
            ret += strlen(const_cast<char*>(out));
        }
    }
    terminal_putchar('\n');
    lua_pushnumber(st, ret);
    return 1;
}

void test_process_1() {
    terminal_writestring("Initializing ACPI.\n");
    initialize_acpi();
    
    kprintf("Initializing PS/2 controller.\n");
    ps2_controller_init();
    
    kprintf("Initializing PS/2 keyboard.\n");
    ps2_keyboard_initialize();
    
    kprintf("Initializing PCI.\n");
    pci_check_bus(0);
    uint32_t child_pid = fork();
    if( child_pid == -1 ) {
        kprintf("Whoops, something went wrong with fork!");
    } else if( child_pid == 0 ) {
        kprintf("Hello from (child) process %u!\n", (unsigned int)process_current->id);
        set_message_listen_status( "keypress", true );
        while(true) {
            unique_ptr<ps2_keypress> kp;
            kp = ps2_keyboard_get_keystroke();
            if( !kp->released ) {
                if( kp->key == KEY_CurUp ) {
                    kprintf("Process %u: Up!\n", process_current->id);
                } else if( kp->key == KEY_CurDown ) {
                    kprintf("Process %u: Down!\n", process_current->id);
                }
            }
        }
    } else {
        kprintf("Hello from (parent) process %u!\n", process_current->id);
        kprintf("Starting lua interpretation.\n");
        lua_State *st = luaL_newstate();
        kprintf("Opened state..\n");
        
        luaL_openlibs( st );
        kprintf("Opened libs..\n");
        lua_register( st, "writeout", lua_writeout_proxy );
        kprintf("Registered writeout..\n");
        int stat = luaL_loadstring( st, "writeout(\"hello from lua version \",_VERSION,\"!\") return 0xC0DE" );
        kprintf("Loaded test program..\n");
        
        if( stat == 0 ) { // was it loaded successfully?
            kprintf("Performing call..\n");
            stat = lua_pcall( st, 0, 1, 0 );
            if( stat == 0 ) { // get return values
                int retval = lua_tonumber(st, -1);
                kprintf("Test program returned successfully, returned value: 0x%x\n", retval);
                lua_pop(st, 1);
            }
        }
        if( stat != 0 ) { // was there an error?
            const char *err = lua_tostring(st, -1);
            kprintf("lua-error: %s\n", err);
            lua_pop(st, 1);
        }
        lua_close(st);
        kprintf("Closed state..\n");
    }
}

void test_process_2() {
    volatile uint32_t loop_var = 0;
    while(true) {
        loop_var++;
        kprintf("Process 2! loop_var = %u.\n", (unsigned long long int)loop_var);
    }
}

__attribute__ ((constructor)) void test_constructor() {
    kprintf("Yes, global constructors *should* be called.\n");
}

extern void* __CTOR_LIST__;

#if defined(__cplusplus)
extern "C" {
#endif
extern void _init(void);
void kernel_init(multiboot_info_t* mb_info, unsigned int magic) {
    terminal_initialize();
    terminal_writestring("Project Omamori now starting...\n");
    gdt_init();
    idt_init();
    initialize_vmem_allocator();
    k_heap_init();
    initialize_pageframes(mb_info);
    
    // do global constructor setup
    kprintf("Calling global constructors.\n");
    size_t *current = (size_t*)((size_t)&__CTOR_LIST__+4); // skip the first function pointer
    int n_constructors_called = 1;
    while(true) {
        if(*current == 0)
            break;
        //kprintf("Calling constructor %u at address 0x%x.\n", (unsigned long long int)n_constructors_called, ((unsigned long long int)*current) );
        void(*func)(void) = (void(*)(void))(*current);
        func();
        current++;
        n_constructors_called++;
    }
    kprintf("Called %u global constructors.\n", (unsigned long long int)n_constructors_called);
    //system_halt
}

void kernel_main(multiboot_info_t* mb_info, unsigned int magic)
{
    char *test;
    int test2 = 0x12345678;
    page_frame* framez;
    page_frame* framez2;
    
    //system_halt;
    kprintf("Kernel begins at physical address 0x%x, corresponding to pageframe ID %u.\n", (unsigned long long int)(&kernel_start_phys), (unsigned long long int)(pageframe_get_block_from_addr( (size_t)&kernel_start_phys )) );
    kprintf("Kernel ends at physical address 0x%x, corresponding to pageframe ID %u.\n", (unsigned long long int)(&kernel_end_phys), (unsigned long long int)(pageframe_get_block_from_addr( (size_t)&kernel_end_phys )) );
    
    terminal_writestring("\nInitializing PICs.\n");
    pic_initialize(PIC_IRQ_OFFSET_1, PIC_IRQ_OFFSET_2);
    set_all_irq_status(true);
    
    terminal_writestring("Initializing PIT.\n");
    pit_initialize(PIT_DEFAULT_FREQ_DIVISOR);
    
    kprintf("Initializing multitasking.\n");
    process *proc1 = new process( (size_t)&test_process_1, false, 0, "test_process_1", NULL, 0 );
    //process *proc2 = new process( (size_t)&test_process_2, false, 0, "test_process_2", NULL, 0 );
    initialize_ipc();
    initialize_multitasking( proc1 );
    //spawn_process( proc2 );
        
    //terminal_writestring("Initializing serial interface.\n");
    //initialize_serial(COM1_BASE_PORT, 3);
    
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
    
    kprintf("Testing kernel vmem allocation.\n");
    size_t vmem_test = k_vmem_alloc( 5 );
    kprintf("Virtual allocation starts at address 0x%x.\n", (unsigned long long int)vmem_test);
    k_vmem_free( vmem_test );
    
    // Remap the VGA memory space.
    // This way, an errant terminal_writestring doesn't bring down the entire system.
    kprintf("Remapping VGA memory.\n");
    size_t vga_vmem = k_vmem_alloc( 1 );
    paging_set_pte( vga_vmem, 0xB8000, 0x101 );
    terminal_buffer = (uint16_t*)vga_vmem;
    kprintf("VGA buffer remapped to 0x%x.\n", (unsigned long long int)vga_vmem);
    
    kprintf("Initiating page fault!\n");
    int *pf_test = (int*)(0xC0F00004);
    *pf_test = 5;
    __sync_synchronize();
    kprintf("Memory read-back test: %u (should be 5)\n", (unsigned long long int)*(int*)(0xC0F00004));
    
    kprintf("Setup complete, starting processes!\n");
    multitasking_start_init();
    
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

#if defined(__cplusplus)
}
#endif