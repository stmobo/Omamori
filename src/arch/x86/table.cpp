#include "includes.h"
#include "arch/x86/isr.h"
#include "device/vga.h"
#include "arch/x86/table.h"
#include "device/pic.h"
#include "arch/x86/multitask.h"

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
uint8_t umode_tss[sizeof(tss)];
idt_entry idt_structs[256];
gdt_entry gdt_structs[NUM_ENTRIES_GDT];
tss active_tss;

void tss::write( uint8_t *dest ) {
    uint16_t *d16 = (uint16_t*)dest;
    uint32_t *d32 = (uint32_t*)dest;
    d16[0]  = this->link;
    
    d32[1]  = this->esp0;
    d16[4]  = this->ss0;
    
    d32[3]  = this->esp1;
    d16[8]  = this->ss1;
    
    d32[5]  = this->esp2;
    d16[12] = this->ss2;
    
    d32[7]  = this->regs.cr3;
    d32[8]  = this->regs.eip;
    d32[9]  = this->regs.eflags;
    
    d32[10] = this->regs.eax;
    d32[11] = this->regs.ecx;
    d32[12] = this->regs.edx;
    d32[13] = this->regs.ebx;
    
    d32[14] = this->regs.esp;
    d32[15] = this->regs.ebp;
    d32[16] = this->regs.esi;
    d32[17] = this->regs.edi;
    
    d16[36] = this->sregs.es;
    d16[38] = this->sregs.cs;
    d16[40] = this->sregs.ss;
    d16[42] = this->sregs.ds;
    d16[44] = this->sregs.fs;
    d16[46] = this->sregs.gs;
    
    d16[48] = this->ldt;
    d16[49] = this->iopb;
}

void tss::load_active() {
    return this->write( (uint8_t*)umode_tss );
}

void tss::read( uint8_t *src ) {
    uint16_t *d16 = (uint16_t*)src;
    uint32_t *d32 = (uint32_t*)src;
    this->link        = d16[0];
    
    this->esp0        = d32[1];
    this->ss0         = d16[4];
    
    this->esp1        = d32[3];
    this->ss1         = d16[8];
    
    this->esp2        = d32[5];
    this->ss2         = d16[12];
    
    this->regs.cr3    = d32[7];
    this->regs.eip    = d32[8];
    this->regs.eflags = d32[9];
    
    this->regs.eax    = d32[10];
    this->regs.ecx    = d32[11];
    this->regs.edx    = d32[12];
    this->regs.ebx    = d32[13];
    
    this->regs.esp    = d32[14];
    this->regs.ebp    = d32[15];
    this->regs.esi    = d32[16];
    this->regs.edi    = d32[17];
    
    this->sregs.es    = d16[36];
    this->sregs.cs    = d16[38];
    this->sregs.ss    = d16[40];
    this->sregs.ds    = d16[42];
    this->sregs.fs    = d16[44];
    this->sregs.gs    = d16[46];
    
    this->ldt         = d16[48];
    this->iopb        = d16[49];
}

void tss::read_active() {
    return this->read( (uint8_t*)umode_tss );
}

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
#ifdef DEBUG
    kprintf("writing to GDT, offset=0x%x\n", (size_t)offset_index);
#endif
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
#ifdef DEBUG
    kprintf("Loading global descriptor table.\ngdt=*(0x%x)\n", (size_t)gdt);
#endif
    
    // GDT null descriptor
    gdt_structs[0].base = 0;
    gdt_structs[0].limit = 0;
    gdt_structs[0].access = 0;
    gdt_structs[0].flags = 0;
    
    // GDT kcode descriptor
    gdt_structs[GDT_KCODE_SEGMENT].base = 0;
    gdt_structs[GDT_KCODE_SEGMENT].limit = (uint32_t)0xFFFFFFFF;
    gdt_structs[GDT_KCODE_SEGMENT].access = GDT_ACCESS_PRESENT | GDT_ACCESS_PRIV0 | GDT_ACCESS_EX | GDT_ACCESS_RW;
    gdt_structs[GDT_KCODE_SEGMENT].flags = 0;
    
    // GDT kdata descriptor
    gdt_structs[GDT_KDATA_SEGMENT].base = 0;
    gdt_structs[GDT_KDATA_SEGMENT].limit = (uint32_t)0xFFFFFFFF;
    gdt_structs[GDT_KDATA_SEGMENT].access = GDT_ACCESS_PRESENT | GDT_ACCESS_PRIV0 | GDT_ACCESS_RW;
    gdt_structs[GDT_KDATA_SEGMENT].flags = 0;
    
    // GDT ucode descriptor
    gdt_structs[GDT_UCODE_SEGMENT].base = 0;
    gdt_structs[GDT_UCODE_SEGMENT].limit = (uint32_t)0xFFFFFFFF;
    gdt_structs[GDT_UCODE_SEGMENT].access = GDT_ACCESS_PRESENT | GDT_ACCESS_PRIV3 | GDT_ACCESS_EX | GDT_ACCESS_RW;
    gdt_structs[GDT_UCODE_SEGMENT].flags = 0;
    
    // GDT udata descriptor
    gdt_structs[GDT_UDATA_SEGMENT].base = 0;
    gdt_structs[GDT_UDATA_SEGMENT].limit = (uint32_t)0xFFFFFFFF;
    gdt_structs[GDT_UDATA_SEGMENT].access = GDT_ACCESS_PRESENT | GDT_ACCESS_PRIV3 | GDT_ACCESS_RW;
    gdt_structs[GDT_UDATA_SEGMENT].flags = 0;
    
    // GDT TSS descriptor
    gdt_structs[GDT_TSS_SEGMENT].base = (size_t)&umode_tss;
    gdt_structs[GDT_TSS_SEGMENT].limit = sizeof(tss);
    gdt_structs[GDT_TSS_SEGMENT].access = GDT_ACCESS_PRESENT | GDT_ACCESS_EX | 0x1;
    gdt_structs[GDT_TSS_SEGMENT].flags = 4; // size bit
    
    memclr(&umode_tss, sizeof(tss));
    
    sync_gdt();
    
    loadGDT((size_t)8*NUM_ENTRIES_GDT-1, (size_t)gdt);
    
    uint32_t gdt_base = getGDT_base();
#ifdef DEBUG
    kprintf("GDT is now located at 0x%x\n", gdt_base);
    terminal_writestring("Now reloading segment registers.\n");
#endif
    reload_seg_registers();
}

void idt_init() {
#ifdef DEBUG
    kprintf("Loading interrupt descriptor table.\nidt=*(0x%x)\n", (size_t)idt);
#endif
    
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
    
    idt_structs[0x5C].offset = (size_t)&__syscall_entry;
    idt_structs[0x5C].type_attr = IDT_ATTR_PRESENT | IDT_ATTR_PRIV3 | IDT_INT_GATE_32;
    idt_structs[0x5C].selector = 0x08;
    
    sync_idt();
    
    loadIDT((size_t)0x800-1, (size_t)idt);
    
#ifdef DEBUG
    uint32_t idt_base = getIDT_base();
    kprintf("IDT is now located at 0x%x.\n", idt_base);
#endif
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