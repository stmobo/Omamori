# multitask_ll.s

.global as_syscall
.global syscall_num
.global reg_dump_area
.global __syscall_entry
.global __multitasking_kmode_entry
.global do_context_switch
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
#    uint16_t ss;      // 50
#    uint32_t cr3;     // 52
#    uint32_t kern_esp;// 56
#} cpu_regs;

# ... program stack ...
# Saved EFLAGS
# Saved CS
# Saved EIP

__syscall_entry:
    movl $1, (as_syscall)
    mov %eax, (syscall_num)
    # now fall through to below
    
__multitasking_kmode_entry:
    # we are working off of esp0 / ss0 in active_tss.
    mov %eax, (reg_dump_area)
    mov $reg_dump_area, %eax
    # save "easy" registers
    mov %ebx, 4(%eax)
    mov %ecx, 8(%eax)
    mov %edx, 12(%eax)
    mov %esi, 16(%eax)
    mov %edi, 20(%eax)
    mov %ebp, 36(%eax)
    mov %cr3, %ebx
    mov %ebx, 52(%eax)
    
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
    mov %ss, 50(%eax)

    # save EIP
    mov 4(%esp), %ebx
    mov %ebx, 24(%eax)
    
    # save ESP (pre-interrupt)
    mov %esp, %ebx
    add $12, %ebx
    mov %ebx, 32(%eax)
    
    mov $0x10, %eax
    mov %eax, %ds
    mov %eax, %es
    mov %eax, %fs
    mov %eax, %gs

    mov (syscall_num), %ebx
    push %ebx
    
    call do_context_switch
    
    pop %ebx
    
    # clear syscall number / state
    movl $0, (as_syscall)
    movl $0, (syscall_num)
    
    # were we in a syscall to begin with?
    # (i.e was syscall_num set to something?)
    cmp $0, %ebx
    jne __process_load_registers # if we were, then don't do IRQ0 code
    
    mov $reg_dump_area, %eax
    
    # push args for call to do_irq:
    # CS, EIP and irq_num (in that order)
    
    # CS
    mov $0, %ebx
    mov 40(%eax), %bx
    push %ebx
    
    # EIP
    mov 24(%eax), %ebx
    push %ebx
    
    # irq_num
    pushl $0
    
    call do_irq
    
    add $12, %esp
    
__process_load_registers:
    mov $reg_dump_area, %eax
    
    # load cr3
    mov 52(%eax), %ebx
    mov %ebx, %cr3
    
    # load EBP
    mov 36(%eax), %ebp
    
    mov $0, %ebx
    mov 40(%eax), %bx
    
    # were we in kernel mode to begin with?
    cmp $0x08, %bx
    je .__process_kmode_stack_load
    
    # load SS for iret (only when coming from usermode)
    mov $0, %ebx
    mov 50(%eax), %bx
    push %ebx
    
    # load ESP for iret (only when coming from usermode)
    mov 32(%eax), %ebx
    push %ebx
    jmp .__process_load_rest
    
.__process_kmode_stack_load:
    # don't need to load SS
    
    # load ESP
    mov 32(%eax), %esp
    
    # and fall through

.__process_load_rest:
    # load EFLAGS for iret (this will also reenable interrupts if they were before the context switch)
    mov 28(%eax), %ebx
    push %ebx
    
    # load CS for iret
    mov $0, %ebx
    mov 40(%eax), %bx
    push %ebx
    
    # load EIP for iret
    mov 24(%eax), %ebx
    push %ebx
    
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
    mov (reg_dump_area), %eax
    mov (reg_dump_area+42), %ds
    
    iret # this will load SS:ESP (if needed), EFLAGS, and CS:EIP for us
    
# we can't rely on the iret push order here.
# however, we can rely on a pushed EIP value at 4(%esp).
__save_registers_non_int:
    mov %eax, (reg_dump_area)
    mov $reg_dump_area, %eax
    # save "easy" registers
    mov %ebx, 4(%eax)
    mov %ecx, 8(%eax)
    mov %edx, 12(%eax)
    mov %esi, 16(%eax)
    mov %edi, 20(%eax)
    mov %ebp, 36(%eax)
    mov %cr3, %ebx
    mov %ebx, 52(%eax)
    
    # save EFLAGS first
    pushf
    pop %ebx
    mov %ebx, 28(%eax)
    
    # make sure we don't get preempted
    cli
    
    # save CS
    mov %cs, 40(%eax)
    
    # save other segment registers
    mov %ds, 42(%eax)
    mov %es, 44(%eax)
    mov %fs, 46(%eax)
    mov %gs, 48(%eax)
    mov %ss, 50(%eax)

    # save EIP
    mov 4(%esp), %ebx
    mov %ebx, 24(%eax)
    
    # save ESP (pre-call)
    mov %esp, %ebx
    add $4, %ebx
    mov %ebx, 32(%eax)
    
    ret
    