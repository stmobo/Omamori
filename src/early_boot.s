.set KERNEL_VIRT_BASE,  0xC0000000
.set KERNEL_PAGE_NUM,  (KERNEL_VIRT_BASE>>22)

.set ALIGN,     1<<0
.set MEMINFO,   1<<1
.set FLAGS,     ALIGN | MEMINFO
.set MAGIC,     0x1BADB002
.set CHECKSUM,  -(MAGIC+FLAGS)

.section .multiboot, "ax"
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.section .boot_page_tables, "ax"
.align 0x1000
.global PageTable0
.set address, 3
PageTable0:
    .rept 1024
    .long address
    .set address, address + 0x1000
    .endr

.align 0x1000
.global PageTable768
.set address, 3
PageTable768:
    .rept 1024
    .long address
    .set address, address + 0x1000
    .endr

.align 0x1000
.global BootPD
BootPD:
    .long (PageTable0+1)
    .rept 767
    .long 0
    .endr
    .long (PageTable768+1)
    .rept 254
    .long 0
    .endr
    .long (BootPD+1)
    
.section .entry, "ax"
.global start
start:
    mov $BootPD, %ecx
    mov %ecx, %cr3
    
    mov %cr0, %ecx
    or $0x80000000, %ecx
    mov %ecx, %cr0
    
    lea (_start_kernel), %ecx
    jmp *%ecx
    