#pragma once
#include "includes.h"

// Set this bit for all valid descriptors
#define GDT_ACCESS_PRESENT 0x80

// decriptor privilege levels (not sure what these do)
// You need to have at least one of these in an access byte, though (there's a required 1 bit that these set as well)
#define GDT_ACCESS_PRIV0   0x10
#define GDT_ACCESS_PRIV1   0x30
#define GDT_ACCESS_PRIV2   0x50
#define GDT_ACCESS_PRIV3   0x70

#define GDT_ACCESS_EX      0x08 // Set for code segments, leave cleared for data segments
#define GDT_ACCESS_DIR     0x04 // "direction" bit, see osdev.org for details
#define GDT_ACCESS_RW      0x02 // Read/Write bit (enables reading for code segments, enables writing for data segs.)

#define GDT_FLAG_PAGE_GR   0x08 // Page granularity bit; clear if using byte granularity, set if using page gran.
#define GDT_FLAG_PAGE_SZ   0x04 // Set for 32bit descriptors, leave cleared for 16bit descriptors

// Set this bit for all used interrupts
#define IDT_ATTR_PRESENT   0x80

// Descriptor privilege levels (specifies which ring the caller should be in before calling)
// Note that if you don't specify one, ring 0 privileges are automatically required (see how IDT_ATTR_PRIV0 is 0x00?)
#define IDT_ATTR_PRIV0   0x00
#define IDT_ATTR_PRIV1   0x20
#define IDT_ATTR_PRIV2   0x40
#define IDT_ATTR_PRIV3   0x60

// set to zero for interrupt gates.
#define IDT_STORAGE_SEG  0x10

// Gate types:
// When a trap (exception) or interrupt occurs that transfers execution to a trap or interrupt gate,
// the CPU will automatically push %eflags, %cs, and %eip to the stack, for later resumption via the "iret" instruction.
// (exceptions may also push an error code.)
// Interrupt gates will automatically disable interrupts upon entry.

// Task gates cause task switches when called.
#define IDT_TASK_GATE    0x5
#define IDT_INT_GATE_16  0x6
#define IDT_TRAP_GATE_16 0x7
#define IDT_INT_GATE_32  0xE
#define IDT_TRAP_GATE_32 0xF

// The number of entries we have in the GDT.
// (the IDT has space for 256 interupts regardless of whether or not we use all 255.)
#define NUM_ENTRIES_GDT  4

/*
extern void encode_idt_entry(uint8_t*, idt_entry);
extern void decode_idt_entry(uint8_t*, idt_entry);
extern void encode_gdt_entry(uint8_t*, gdt_entry);
extern void decode_gdt_entry(uint8_t*, gdt_entry);


extern void write_idt(uint8_t*, uint8_t, idt_entry);
extern void write_gdt(uint8_t*, uint8_t, gdt_entry);

extern void load_idt(table_descriptor*);
extern void load_gdt(table_descriptor*);
*/

extern void gdt_init();
extern void idt_init();
extern bool add_idt_entry(void*, int, bool);
extern bool add_gdt_entry(int, size_t, size_t, char);