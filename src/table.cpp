#include "includes.h"
#include "isr.h"
#include "vga.h"
#include "table.h"
#include "pic.h"

#define add_idt_trap_entry(func, index) \
    idt_structs[index].offset = (size_t)&func; \
    idt_structs[index].type_attr = IDT_ATTR_PRESENT | IDT_ATTR_PRIV0 | IDT_TRAP_GATE_32; \
    idt_structs[index].selector = 0x08;
    
struct gdt_entry {
    size_t base;
    size_t limit;
    char access;
    char flags;
};

struct idt_entry {
    size_t offset;
    short selector;
    char type_attr;
};

extern "C" {
    extern uint16_t getGDT_limit(void);
    extern uint32_t getGDT_base(void);
    extern void loadGDT(size_t, size_t);
    extern uint32_t getIDT_base(void);
    extern uint16_t getIDT_limit(void);
    extern void loadIDT(size_t, size_t);
    extern void reload_seg_registers(void);
};

uint8_t idt[0x800]; // 256 8-byte entries
uint8_t gdt[8*NUM_ENTRIES_GDT];
idt_entry idt_structs[256];
gdt_entry gdt_structs[NUM_ENTRIES_GDT];

void add_irq_entry(int irq_num, size_t func) {
    idt_structs[PIC_IRQ_OFFSET_1+irq_num].offset = func;
    idt_structs[PIC_IRQ_OFFSET_1+irq_num].type_attr = IDT_ATTR_PRESENT | IDT_ATTR_PRIV3 | IDT_INT_GATE_32;
    idt_structs[PIC_IRQ_OFFSET_1+irq_num].selector = 0x08;
}

// encode_*dt_entry - convert a gdt_entry or idt_entry struct to an actual descriptor of the corresponding type.
// decode_*dt_entry - convert a descriptor to a *dt_entry struct.

void encode_idt_entry(uint8_t* dest, idt_entry entry) {
    dest[0] = entry.offset & (uint32_t)0xFF;
    dest[1] = (entry.offset & (uint32_t)0xFF00) >> 8;
    dest[2] = entry.selector & (uint16_t)0xFF;
    dest[3] = (entry.selector & (uint16_t)0xFF00) >> 8;
    dest[4] = 0;
    dest[5] = entry.type_attr;
    dest[6] = (entry.offset & (uint32_t)0xFF0000) >> 16;
    dest[7] = (entry.offset >> 24) & (uint32_t)0xFF;
}

void decode_idt_entry(uint8_t* src, idt_entry entry) {
    entry.offset = (src[0] | (src[1] << 8) | (src[6] << 16) | (src[7] << 24));
    entry.selector = (src[2] | (src[3] << 8));
    entry.type_attr = src[5];
}

void encode_gdt_entry(uint8_t* dest, gdt_entry entry) {
    uint8_t flags = 0;
    if(entry.limit > 65536){
        entry.limit >>= 12;
        flags = 0xC;
    } else {
        flags = 0x4;
    }
    dest[0] = entry.limit & (uint32_t)0x000000FF;
    dest[1] = (entry.limit & (uint32_t)0x0000FF00) >> 8;
    dest[2] = (entry.base & (uint32_t)0x000000FF);
    dest[3] = (entry.base & (uint32_t)0x0000FF00) >> 8;
    dest[4] = (entry.base & (uint32_t)0x00FF0000) >> 16;
    dest[5] = entry.access;
    dest[6] = (((flags) << 4) | ((entry.limit&(uint32_t)0xF0000) >> 16));
    dest[7] = ((entry.base >> 24) & (uint32_t)0xFF);
}

// decode_gdt_entry - convert a GDT entry (in memory) to a gdt_entry struct.
void decode_gdt_entry(uint8_t *src, gdt_entry entry) {
    entry.limit = (src[0] | (src[1]<<8) | ((src[6]&0xF) << 16));
    entry.base = (src[2] | (src[3]<<8) | (src[4]<<16) | (src[7]<<24));
    entry.flags = (src[6]&0xF0)>>4;
    entry.access = src[5];
}

// write_*dt - Write a descriptor structure to memory.
void write_idt(uint8_t* dest, uint16_t index, idt_entry entry) {
    uint8_t* offset_index = (uint8_t*)(dest+(index*8));
    encode_idt_entry(offset_index, entry);
}

void write_gdt(uint8_t* dest, uint8_t index, gdt_entry entry) {
    uint8_t* offset_index = (uint8_t*)(dest+(index*8));
    char hex[9];
    hex[8] = '\0';
    terminal_writestring("writing to GDT, offset= 0x");
    int_to_hex((size_t)offset_index, hex);
    terminal_writestring(hex);
    terminal_putchar('\n');
    encode_gdt_entry(offset_index, entry);
}

// sync_*dt - Write all of the descriptor tables to memory, from the *dt_structs arrays.
void sync_gdt() {
    for(uint8_t i=0;i<NUM_ENTRIES_GDT;i++)
        write_gdt(gdt, i, gdt_structs[i]);
}

void sync_idt() {
    for(uint16_t i=0;i<256;i++)
        write_idt(idt, i, idt_structs[i]);
}

// *dt_init() - Initialize the Global and Interrupt Descriptor tables.
// gdt_init() initializes most of the segment descriptors we'll use for operations throughout.
// idt_init() initializes the exception handlers.
// Both functions then flush their struct arrays to memory (see sync_*dt(), above) and then call their
// assembly loading functions (load<IDT/GDT>()) which execute the requisite "lidt" and "lgdt" instructions.
void gdt_init() {
    char hex[8];
    terminal_writestring("Loading global descriptor table.\n");
    terminal_writestring("\ngdt= *(0x");
    int_to_hex((size_t)gdt, hex);
    terminal_writestring(hex);
    terminal_writestring(")\n");
    
    // GDT null descriptor
    gdt_structs[0].base = 0;
    gdt_structs[0].limit = 0;
    gdt_structs[0].access = 0;
    
    // GDT code descriptor
    gdt_structs[1].base = 0;
    gdt_structs[1].limit = (uint32_t)0xFFFFFFFF;
    gdt_structs[1].access = GDT_ACCESS_PRESENT | GDT_ACCESS_PRIV0 | GDT_ACCESS_EX | GDT_ACCESS_RW;
    
    // GDT data descriptor
    gdt_structs[2].base = 0;
    gdt_structs[2].limit = (uint32_t)0xFFFFFFFF;
    gdt_structs[2].access = GDT_ACCESS_PRESENT | GDT_ACCESS_PRIV0 | GDT_ACCESS_RW;
    
    // GDT TSS descriptor
    gdt_structs[3].base = 0; // fill out later
    gdt_structs[3].limit = 0; // fill out later
    gdt_structs[3].access = GDT_ACCESS_PRESENT | GDT_ACCESS_EX | 0x1;
    
    sync_gdt();
    
    loadGDT((size_t)8*NUM_ENTRIES_GDT-1, (size_t)gdt);
    
    uint32_t gdt_base = getGDT_base();
    
    terminal_writestring("GDT is now located at 0x");
    int_to_hex(gdt_base, hex);
    terminal_writestring(hex, 8);
    terminal_writestring("\nNow reloading segment registers.\n");
    reload_seg_registers();
}

void idt_init() {
    char hex[8];
    terminal_writestring("Loading interrupt descriptor table.\n");
    terminal_writestring("\nidt= *(0x");
    int_to_hex((size_t)idt, hex);
    terminal_writestring(hex);
    terminal_writestring(")\n");
    
    add_idt_trap_entry(_isr_div_zero, 0)
    add_idt_trap_entry(_isr_debug, 1)
    add_idt_trap_entry(_isr_nmi, 2)
    add_idt_trap_entry(_isr_breakpoint, 3)
    add_idt_trap_entry(_isr_overflow, 4)
    add_idt_trap_entry(_isr_boundrange, 5)
    add_idt_trap_entry(_isr_invalidop, 6)
    add_idt_trap_entry(_isr_devnotavail, 7)
    add_idt_trap_entry(_isr_dfault, 8)
    add_idt_trap_entry(_isr_invalidTSS, 10)
    add_idt_trap_entry(_isr_segnotpresent, 11)
    add_idt_trap_entry(_isr_stackseg, 12)
    add_idt_trap_entry(_isr_gpfault, 13)
    add_idt_trap_entry(_isr_pagefault, 14)
    add_idt_trap_entry(_isr_reserved, 15)
    add_idt_trap_entry(_isr_fpexcept, 16)
    add_idt_trap_entry(_isr_align, 17)
    add_idt_trap_entry(_isr_machine, 18)
    add_idt_trap_entry(_isr_simd_fp, 19)
    add_idt_trap_entry(_isr_virt, 20)
    for(int i=21;i<30;i++) {
        add_idt_trap_entry(_isr_reserved, i)
    }
    add_idt_trap_entry(_isr_security, 30)
    add_idt_trap_entry(_isr_test, 0xFF)
    add_irq_entry(0, (size_t)&_isr_irq_0);
    add_irq_entry(1, (size_t)&_isr_irq_1);
    add_irq_entry(2, (size_t)&_isr_irq_2);
    add_irq_entry(3, (size_t)&_isr_irq_3);
    add_irq_entry(4, (size_t)&_isr_irq_4);
    add_irq_entry(5, (size_t)&_isr_irq_5);
    add_irq_entry(6, (size_t)&_isr_irq_6);
    add_irq_entry(7, (size_t)&_isr_irq_7);
    add_irq_entry(8, (size_t)&_isr_irq_8);
    add_irq_entry(9, (size_t)&_isr_irq_9);
    add_irq_entry(10, (size_t)&_isr_irq_10);
    add_irq_entry(11, (size_t)&_isr_irq_11);
    add_irq_entry(12, (size_t)&_isr_irq_12);
    add_irq_entry(13, (size_t)&_isr_irq_13);
    add_irq_entry(14, (size_t)&_isr_irq_14);
    add_irq_entry(15, (size_t)&_isr_irq_15);
    
    
    sync_idt();
    
    loadIDT((size_t)0x800-1, (size_t)idt);
    
    uint32_t idt_base = getIDT_base();
    terminal_writestring("\nIDT is now located at 0x");
    int_to_hex(idt_base, hex);
    terminal_writestring(hex, 8);
    terminal_putchar('\n');
}

// add_*dt_entry - Add a descriptor to the struct array
// Both of these add descriptor structures to the struct arrays, and then flush them to the actual tables using the sync()
// functions.

// void* isr in add_idt_entry refers to the ASSEMBLY ISR (the one that uses "iret" to return), not whatever C function
// you may be using as a handler!
bool add_idt_entry(void* isr, int index, bool is_active) {
    if(index > 30) {
        if(isr != NULL)
            idt_structs[index].offset = (size_t)isr;
        if(is_active)
            idt_structs[index].type_attr = IDT_ATTR_PRESENT | IDT_ATTR_PRIV3 | IDT_INT_GATE_32;
        else
            idt_structs[index].type_attr = IDT_ATTR_PRIV3 | IDT_INT_GATE_32;
        idt_structs[index].selector = 0x08;
        sync_idt();
        return true;
    }
    return false;
}

bool add_gdt_entry(int index, size_t base, size_t limit, char access) {
    if(index > 2) {
        gdt_structs[index].base = base;
        gdt_structs[index].limit = limit & (size_t)0xFFFFF;
        gdt_structs[index].access = access;
        sync_gdt();
        return true;
    }
    return false;
}