#include "includes.h"
#include "arch/x86/irq.h"
#include "arch/x86/pic.h"
#include "arch/x86/table.h"
#include "boot/multiboot.h"
#include "core/paging.h"
#include "device/pit.h"
#include "device/vga.h"

#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

#if !defined(__cplusplus)
#error "This is C++ code, use a C++ compiler."
#endif

__attribute__ ((constructor)) void test_constructor() {
    kprintf("Yes, global constructors *should* be called.\n");
}

extern void* __CTOR_LIST__;

extern "C" {
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
    
    //system_halt;
    kprintf("Kernel begins at physical address 0x%x, corresponding to pageframe ID %u.\n", (unsigned long long int)(&kernel_start_phys), (unsigned long long int)(pageframe_get_block_from_addr( (size_t)&kernel_start_phys )) );
    kprintf("Kernel ends at physical address 0x%x, corresponding to pageframe ID %u.\n", (unsigned long long int)(&kernel_end_phys), (unsigned long long int)(pageframe_get_block_from_addr( (size_t)&kernel_end_phys )) );
    
    terminal_writestring("\nInitializing PICs.\n");
    pic_initialize(PIC_IRQ_OFFSET_1, PIC_IRQ_OFFSET_2);
    set_all_irq_status(true);
    
    terminal_writestring("Initializing PIT.\n");
    pit_initialize(PIT_DEFAULT_FREQ_DIVISOR);
    //system_halt
}
}