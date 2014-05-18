# multitask_ll.s

.global syscall_num
.global active_regs
.global __syscall_entry
.global __multitasking_kmode_entry
.global __usermode_jump

#typedef struct cpu_regs {
#    uint32_t eax;     // 0
#    uint32_t ebx;     // 4
#    uint32_t ecx;     // 8
#    uint32_t edx;     // 12
#    uint32_t esi;     // 16
#    uint32_t edi;     // 20
#    uint32_t eip;     // 24
#    uint32_t eflags;  // 28
#    uint32_t esp;     // 32
#    uint32_t ebp;     // 36
#    uint16_t cs;      // 40
#    uint16_t ds;      // 42
#    uint16_t es;      // 44
#    uint16_t fs;      // 46
#    uint16_t gs;      // 48
#    uint32_t cr3;     // 50
#    uint32_t kern_esp;// 54
#} cpu_regs;

# ... program stack ...
# Saved EFLAGS
# Saved CS
# Saved EIP

__syscall_entry:
    push %ebx
    mov $syscall_num, %ebx
    mov %eax, (%ebx)
    pop %ebx
    # now fall through to below
    
__multitasking_kmode_entry:
    # we are working off of esp0 / ss0 in active_tss.
    mov %eax, (active_regs)
    mov $active_regs, %eax
    # save "easy" registers
    mov %ebx, 4(%eax)
    mov %ecx, 8(%eax)
    mov %edx, 12(%eax)
    mov %esi, 16(%eax)
    mov %edi, 20(%eax)
    mov %ebp, 36(%eax)
    mov %cr3, %ebx
    mov %ebx, 50(%eax)
    
    # save EFLAGS first
    mov 12(%esp), %ebx
    mov %ebx, 28(%eax)
    
    # make sure we don't get preempted
    cli
    
    # save CS
    mov 8(%esp), %ebx
    mov %ebx, 40(%eax)
    
    # save other segment registers
    mov %ds, 42(%eax)
    mov %es, 44(%eax)
    mov %fs, 46(%eax)
    mov %gs, 48(%eax)

    # save EIP
    mov 4(%esp), %ebx
    mov %ebx, 24(%eax)
    
    # save ESP (pre-interrupt)
    mov %esp, %ebx
    add $16, %ebx
    mov %ebx, 32(%eax)
    
    mov $0x10, %eax
    mov %eax, %ds
    mov %eax, %es
    mov %eax, %fs
    mov %eax, %gs

    call do_context_switch
    # were we in a syscall?
    mov $syscall_num, %eax
    mov (%eax), %ebx
    cmp $0, %ebx
    jne .__skip_syscall_clear
    
    # if so, then clear the number
    mov $0, %ebx
    mov %ebx, (%eax)
    
.__skip_syscall_clear:
    mov $active_regs, %eax
    
    # load ESP / EBP
    mov 32(%eax), %esp
    mov 36(%eax), %ebp
    
    # load EIP for iret
    mov 24(%eax), %ebx
    mov %ebx, 4(%esp)
    
    # load CS for iret
    mov $0, %ebx
    mov 40(%eax), %bx
    mov %ebx, 8(%esp)
    
    # load EFLAGS for iret (this will also reenable interrupts if they were before the context switch)
    mov 28(%eax), %ebx
    mov %ebx, 12(%esp)
    
    # load cr3
    mov 50(%eax), %ebx
    mov %ebx, %cr3
    
    # reload general registers
    mov 4(%eax), %ebx
    mov 8(%eax), %ecx
    mov 12(%eax), %edx
    mov 16(%eax), %esi
    mov 20(%eax), %edi
    
    # load segment registers
    mov 44(%eax), %es
    mov 46(%eax), %fs
    mov 48(%eax), %gs
    
    # load EAX
    mov (active_regs), %eax
    mov (active_regs+42), %ds
    
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
