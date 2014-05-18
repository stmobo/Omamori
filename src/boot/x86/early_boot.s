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
.set address, 0x103 # Global, Read/Write, Present
PageTable0:
    .rept 1024
    .long address
    .set address, address + 0x1000
    .endr

.align 0x1000
.global PageTable768
.set address, 0x103
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
    # load CR3 with the page directory address
    mov $BootPD, %ecx
    mov %ecx, %cr3
    
    # enable global page sharing
    mov %cr4, %ecx
    or $8, %ecx
    mov %ecx, %cr4
    
    # enable paging
    mov %cr0, %ecx
    or $0x80000000, %ecx
    mov %ecx, %cr0
    
    # jump to higher-half code
    lea (_start_kernel), %ecx
    jmp *%ecx
    