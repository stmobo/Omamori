# multitask_ll.s

.global active_regs
.global __syscall_entry
.global __ctext_switch_entry
.global __usermode_jump

# struct order:
# uint32_t eax;
# uint32_t ebx;
# uint32_t ecx;
# uint32_t edx;
# uint32_t esi;
# uint32_t edi;
# uint32_t eip;
# uint32_t eflags;

# ... program stack ...
# Saved EFLAGS
# Saved CS
# Saved EIP

__syscall_entry:
    mov %eax, (active_regs)
    mov $active_regs, %eax
    # push "easy" registers
    mov %ebx, 4(%eax)
    mov %ecx, 8(%eax)
    mov %edx, 12(%eax)
    mov %esi, 16(%eax)
    mov %edi, 20(%eax)
    mov %ebp, 36(%eax)
    
    push %ebx
    
    # save EIP
    mov 4(%esp), %ebx
    mov %ebx, 24(%eax)
    
    # save EFLAGS 
    mov 12(%esp), %ebx
    mov %ebx, 28(%eax)
    
    # save ESP (pre-interrupt)
    mov %esp, %ebx
    add $16, %ebx
    
    mov %ebx, 32(%eax)
    
    pop %ebx
    
    # load data segment registers for kernel mode
    mov $0x10, %eax
    mov %eax, %ds
    mov %eax, %es
    mov %eax, %fs
    mov %eax, %gs
    
    call do_syscall
    mov %eax, (active_regs)
    mov $active_regs, %eax
    
    # reload easy registers
    mov 4(%eax), %ebx
    mov 8(%eax), %ecx
    mov 12(%eax), %edx
    mov 16(%eax), %esi
    mov 20(%eax), %edi
    mov 32(%eax), %esp
    mov 36(%eax), %ebp
    
    # load data segment registers for user mode
    mov $0x23, %eax
    mov %eax, %ds
    mov %eax, %es
    mov %eax, %fs
    mov %eax, %gs
    mov $active_regs, %eax
    
    # load EIP for iret
    push %ebx
    mov 24(%eax), %ebx
    mov %ebx, 4(%esp)
    
    # load EFLAGS for iret
    mov 28(%eax), %ebx
    mov %ebx, 12(%esp)
    pop %ebx
    
    # load EAX
    mov (active_regs), %eax
    
    iret
    
__ctext_switch_entry:
    mov %eax, (active_regs)
    mov $active_regs, %eax
    # push "easy" registers
    mov %ebx, 4(%eax)
    mov %ecx, 8(%eax)
    mov %edx, 12(%eax)
    mov %esi, 16(%eax)
    mov %edi, 20(%eax)
    mov %ebp, 36(%eax)
    
    push %ebx
    
    # save EIP
    mov 4(%esp), %ebx
    mov %ebx, 24(%eax)
    
    # save EFLAGS 
    mov 12(%esp), %ebx
    mov %ebx, 28(%eax)
    
    # save ESP (pre-interrupt)
    mov %esp, %ebx
    add $16, %ebx
    
    mov %ebx, 32(%eax)
    
    pop %ebx
    
    # load data segment registers for kernel mode
    mov $0x10, %eax
    mov %eax, %ds
    mov %eax, %es
    mov %eax, %fs
    mov %eax, %gs
    
    call do_context_switch
    mov $active_regs, %eax
    
    # reload easy registers
    mov 4(%eax), %ebx
    mov 8(%eax), %ecx
    mov 12(%eax), %edx
    mov 16(%eax), %esi
    mov 20(%eax), %edi
    mov 32(%eax), %esp
    mov 36(%eax), %ebp
    
    # load data segment registers for user mode
    mov $0x23, %eax
    mov %eax, %ds
    mov %eax, %es
    mov %eax, %fs
    mov %eax, %gs
    mov $active_regs, %eax
    
    # load EIP for iret
    push %ebx
    mov 24(%eax), %ebx
    mov %ebx, 4(%esp)
    
    # load EFLAGS for iret
    mov 28(%eax), %ebx
    mov %ebx, 12(%esp)
    pop %ebx
    
    # load EAX
    mov (active_regs), %eax
    
    iret

__usermode_jump:
    # get parameters
    mov 16(%esp), %edx # new stackpointer
    mov 12(%esp), %eax # data seg
    mov 8(%esp),  %ebx # code seg
    mov 4(%esp),  %ecx # func address
    
    # load umode sregs
    or $3, %eax
    or $3, %ebx
    
    mov %eax, %ds
    mov %eax, %es
    mov %eax, %fs
    mov %eax, %gs
    
    # set stuff up for IRET
    push $0x23      # SS
    push %edx       # ESP
    pushf           # flags
    or $3, (%esp)   # reenable interrupts upon IRET
    push %ebx       # CS
    push %ecx       # EIP
    
    # go!
    iret
