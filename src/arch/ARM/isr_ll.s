.global multitasking_enabled
.global multitasking_timeslice_tick_count
.global as_syscall
.global syscall_num

# generic wrapper stuff
_isr_call_cpp_func:
    # make sure we don't mess up any running programs
    #-16:  Return EFLAGS
    #-12:  Return CS
    # -8:  Return EIP
    # -4:  Exception number / error code
    # ESP: Function address
    pusha
    
    # -48: EFLAGS
    # -44: Return CS
    # -40: Return Address
    # -36: ErrCode
    # -32: Exception Handler Address
    # -28: EAX
    # -24: ECX
    # -20: EDX
    # -16: EBX
    # -12: ESP (pre-call)
    # -8 : EBP
    # -4 : ESI
    # ESP: EDI
    
    movl 32(%esp), %eax # get func addr
    movl 36(%esp), %ebx # get err code (ISR vector no. if no err code)
    movl 40(%esp), %ecx # saved EIP
    movl 44(%esp), %edx # saved CS
    push %edx
    push %ecx
    push %ebx
    call *%eax # call cpp handler func
    add $12, %esp
    
    popa
    add $8, %esp
    iret
    
# Before any ISR code runs, our stack looks like:
# ... program stack ...
# Saved EFLAGS
# Saved CS
# Saved EIP
# Error code (if given)
#
# %esp will be pointing to either the saved EIP or to the error code, if given.
    
_isr_div_zero:
    push $0
    push $do_isr_div_zero
    jmp _isr_call_cpp_func
    
_isr_debug:
    push $1
    push $do_isr_debug
    jmp _isr_call_cpp_func

_isr_nmi:
    push $3
    push $do_isr_nmi
    jmp _isr_call_cpp_func
    
_isr_breakpoint:
    push $4
    push $do_isr_breakpoint
    jmp _isr_call_cpp_func
    
_isr_overflow:
    push $5
    push $do_isr_overflow
    jmp _isr_call_cpp_func
    
_isr_boundrange:
    push $6
    push $do_isr_boundrange
    jmp _isr_call_cpp_func
    
_isr_invalidop:
    push $7
    push $do_isr_invalidop
    jmp _isr_call_cpp_func
    
_isr_devnotavail:
    push $8
    push $do_isr_devnotavail
    jmp _isr_call_cpp_func
    
_isr_dfault:
    # already have an err code
    push $do_isr_dfault
    jmp _isr_call_cpp_func
    
_isr_invalidTSS:
    push $do_isr_invalidTSS
    jmp _isr_call_cpp_func
    
_isr_segnotpresent:
    push $do_isr_segnotpresent
    jmp _isr_call_cpp_func
    
_isr_stackseg:
    push $do_isr_stackseg
    jmp _isr_call_cpp_func
    
_isr_gpfault:
    push $do_isr_gpfault
    jmp _isr_call_cpp_func
    
_isr_pagefault:
    push $do_isr_pagefault
    jmp _isr_call_cpp_func
    
_isr_fpexcept:
    push $15
    push $do_isr_fpexcept
    jmp _isr_call_cpp_func
    
_isr_align:
    push $do_isr_align
    jmp _isr_call_cpp_func
    
_isr_machine:
    push $17
    push $do_isr_machine
    jmp _isr_call_cpp_func
    
_isr_simd_fp:
    push $18
    push $do_isr_simd_fp
    jmp _isr_call_cpp_func
    
_isr_virt:
    push $19
    push $do_isr_virt
    jmp _isr_call_cpp_func
    
_isr_security:
    push $20
    push $do_isr_security
    jmp _isr_call_cpp_func
    
_isr_reserved:
    push $0xFF
    push $do_isr_reserved
    jmp _isr_call_cpp_func
    
_isr_test:
    push $0x8D
    push $do_isr_test
    jmp _isr_call_cpp_func

# special treatment for this IRQ
_isr_irq_0:
    # decrement the task switch timer
    push %eax
    mov (multitasking_enabled), %eax
    cmp $0, %eax
    je .__isr_irq_0_no_multitasking
    
    mov (multitasking_timeslice_tick_count), %eax
    # is it time to switch?
    cmp $0, %eax
    jne .__isr_irq_0_no_ctext_switch # do a "normal" irq call if it isn't
    
    pop %eax
    movl $0, (as_syscall)
    movl $0, (syscall_num)
    movl $0, (syscall_arg1)
    movl $0, (syscall_arg2)
    movl $0, (syscall_arg3)
    movl $0, (syscall_arg4)
    movl $0, (syscall_arg5)
    jmp __multitasking_kmode_entry # do note that __multitasking_kmode_entry calls the irq handler in our stead.
    # not falling through -- __multitasking_kmode_entry does the iret itself
    
.__isr_irq_0_no_ctext_switch:
    dec %eax
    mov %eax, (multitasking_timeslice_tick_count) # store new tick count
.__isr_irq_0_no_multitasking:
    pop %eax
    push $0
    push $do_irq
    jmp _isr_call_cpp_func
    
_isr_irq_1:
    push $1
    push $do_irq
    jmp _isr_call_cpp_func
    
_isr_irq_2:
    push $2
    push $do_irq
    jmp _isr_call_cpp_func
    
_isr_irq_3:
    push $3
    push $do_irq
    jmp _isr_call_cpp_func
    
_isr_irq_4:
    push $4
    push $do_irq
    jmp _isr_call_cpp_func

_isr_irq_5:
    push $5
    push $do_irq
    jmp _isr_call_cpp_func
    
_isr_irq_6:
    push $6
    push $do_irq
    jmp _isr_call_cpp_func
    
_isr_irq_7:
    push $7
    push $do_irq
    jmp _isr_call_cpp_func
    
_isr_irq_8:
    push $8
    push $do_irq
    jmp _isr_call_cpp_func
    
_isr_irq_9:
    push $9
    push $do_irq
    jmp _isr_call_cpp_func
    
_isr_irq_10:
    push $10
    push $do_irq
    jmp _isr_call_cpp_func
    
_isr_irq_11:
    push $11
    push $do_irq
    jmp _isr_call_cpp_func
    
_isr_irq_12:
    push $12
    push $do_irq
    jmp _isr_call_cpp_func
    
_isr_irq_13:
    push $13
    push $do_irq
    jmp _isr_call_cpp_func
    
_isr_irq_14:
    push $14
    push $do_irq
    jmp _isr_call_cpp_func
    
_isr_irq_15:
    push $15
    push $do_irq
    jmp _isr_call_cpp_func
    
.globl _isr_div_zero
.globl _isr_debug
.globl _isr_nmi
.globl _isr_breakpoint
.globl _isr_overflow
.globl _isr_boundrange
.globl _isr_invalidop
.globl _isr_devnotavail
.globl _isr_dfault
.globl _isr_invalidTSS
.globl _isr_segnotpresent
.globl _isr_stackseg
.globl _isr_gpfault
.globl _isr_pagefault
.globl _isr_fpexcept
.globl _isr_align
.globl _isr_machine
.globl _isr_simd_fp
.globl _isr_virt
.globl _isr_security
.globl _isr_reserved
.globl _isr_test
.globl _isr_irq_0
.globl _isr_irq_1
.globl _isr_irq_2
.globl _isr_irq_3
.globl _isr_irq_4
.globl _isr_irq_5
.globl _isr_irq_6
.globl _isr_irq_7
.globl _isr_irq_8
.globl _isr_irq_9
.globl _isr_irq_10
.globl _isr_irq_11
.globl _isr_irq_12
.globl _isr_irq_13
.globl _isr_irq_14
.globl _isr_irq_15
