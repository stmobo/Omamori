# Misc. systems stuff that has to be in assembly.
# most of this has to deal with the GDT / IDT for now.

# loadGDT: load global descriptor table
# parameters: (uint16_t limit, uint32_t gdt_ptr)

gdt_desc:
.space 2
gdt_desc_limit:
.space 2
gdt_desc_ptr:
.space 4
loadGDT:
    mov 4(%esp), %dx # limit
    mov 8(%esp), %eax # gdt_ptr
    
    mov %dx, gdt_desc_limit
    mov %eax, gdt_desc_ptr
    lgdt gdt_desc_limit
    ret
    
# loadIDT: load interrupt descriptor tables
# parameters: (uint16_t limit, uint32_t idt_ptr)
    
idt_desc:
.space 2
idt_desc_limit:
.space 2
idt_desc_ptr:
.space 4
loadIDT:
    mov 4(%esp), %dx # limit
    mov 8(%esp), %eax # idt_ptr
    
    mov %dx, idt_desc_limit
    mov %eax, idt_desc_ptr
    lidt idt_desc_limit
    ret

# dump various parts of the gdt or idt registers to memory
getGDT_base:
    sgdt gdt_desc_limit
    mov gdt_desc_ptr, %eax
    ret
    
getGDT_limit:
    sgdt gdt_desc_limit
    mov gdt_desc_limit, %ax
    ret
    
getIDT_base:
    sidt idt_desc_limit
    mov idt_desc_ptr, %eax
    ret
    
getIDT_limit:
    sidt idt_desc_limit
    mov idt_desc_limit, %ax
    ret
    
# reload_seg_registers: load segment registers with default values.
reload_seg_registers:
    .intel_syntax
    jmp 0x08:still_reloading # set CS
    .att_syntax
still_reloading:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    ret

.globl getGDT_base
.globl getGDT_limit
.globl getIDT_base
.globl getIDT_limit
.globl loadGDT
.globl loadIDT
.globl reload_seg_registers
