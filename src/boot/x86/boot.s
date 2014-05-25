.section .bootstrap_stack, "aw", @nobits
.align 32
stack_bottom:
.skip 16384
stack_top:

.section .text, "ax"
.global _start_kernel
.type _start_kernel, @function
_start_kernel:
    # whoo kernel mode!
    movl $stack_top, %esp
    mov $stack_top, %ebp
    
    # push multiboot struct addr / magic number
    push %eax
    add $0xC0000000, %ebx
    push %ebx
    
    call kernel_main
    pop %ebx
    pop %eax
    
    sub 4, %esp
    movl $0, (%esp)
    
    call __cxa_finalize
    
    add 4, %esp
    
.Lhang:
    hlt
    jmp .Lhang
    
.size _start_kernel, . - _start_kernel
