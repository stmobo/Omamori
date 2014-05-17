/******************************************************************************
 *
 * Name: aclinux.h - OS specific defines, etc. for Linux
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2014, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights. You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (with the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code. No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision. In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change. Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee. Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution. In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE. ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT, ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES. INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS. INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES. THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government. In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************/

#ifndef __ACOMAMORI_H__
#define __ACOMAMORI_H__

/* Common (in-kernel/user-space) ACPICA configuration */

#define ACPI_MUTEX_TYPE             ACPI_BINARY_SEMAPHORE

#define ACPI_USE_SYSTEM_INTTYPES

/* External globals for __KERNEL__, stubs is needed */

#define ACPI_GLOBAL(t,a)
#define ACPI_INIT_GLOBAL(t,a,b)

/* Generating stubs for configurable ACPICA macros */

#define ACPI_NO_MEM_ALLOCATIONS

/* Generating stubs for configurable ACPICA functions */

#define ACPI_NO_ERROR_MESSAGES
#undef ACPI_DEBUG_OUTPUT

/* External interface for __KERNEL__, stub is needed */

#define ACPI_EXTERNAL_RETURN_STATUS(Prototype) \
    static ACPI_INLINE Prototype {return(AE_NOT_CONFIGURED);}
#define ACPI_EXTERNAL_RETURN_OK(Prototype) \
    static ACPI_INLINE Prototype {return(AE_OK);}
#define ACPI_EXTERNAL_RETURN_VOID(Prototype) \
    static ACPI_INLINE Prototype {return;}
#define ACPI_EXTERNAL_RETURN_UINT32(Prototype) \
    static ACPI_INLINE Prototype {return(0);}
#define ACPI_EXTERNAL_RETURN_PTR(Prototype) \
    static ACPI_INLINE Prototype {return(NULL);}

/* Host-dependent types and defines for in-kernel ACPICA */

#define ACPI_MACHINE_WIDTH          32

#define ACPI_SPINLOCK               spinlock*
#define ACPI_CPU_FLAGS              uint32_t

/* Linux uses GCC */

#include "acgcc.h"

/*
 * FIXME: Inclusion of actypes.h
 * Linux kernel need this before defining inline OSL interfaces as
 * actypes.h need to be included to find ACPICA type definitions.
 * Since from ACPICA's perspective, the actypes.h should be included after
 * acenv.h (aclinux.h), this leads to a inclusion mis-ordering issue.
 */
#include "acpi/actypes.h"

/*
 * Memory allocation/deallocation
 */
 
static inline void *AcpiOsAllocate ( ACPI_SIZE Size )
{
    return (void*)kmalloc(Size);
}
#define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsAllocate

static inline void *AcpiOsAllocateZeroed (ACPI_SIZE Size) {
    char *m = kmalloc(Size);
    if(m != NULL) {
        memclr(m, Size);
        return m;
    }
    return NULL;
}
#define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsAllocateZeroed
#define USE_NATIVE_ALLOCATE_ZEROED

static inline void AcpiOsFree (void *Memory)
{
    kfree((char*)Memory);
}
#define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsFree

ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK *OutHandle) {
    OutHandle = new spinlock; // kinda wasteful, isn't it?
    if(OutHandle == NULL)
        return AE_NO_MEMORY;
    return AE_OK;
}
#define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsCreateLock

void *AcpiOsMapMemory ( ACPI_PHYSICAL_ADDRESS Where, ACPI_SIZE Length) {
    if( Length & 0xFFF ) {
        Length += 0x1000
    }
    Length &= 0xFFFFF000;
    int n_pages = Length / 0x1000;
    size_t vaddr = k_vmem_alloc( n_pages );
    page_frame *paddr = pageframe_allocate_at( Where, n_pages );
    for(int i = 0;i<n_pages;i++) {
        size_t current_address = vaddr+(i*0x1000);
        paging_set_pte( current_address, paddr[i].address, 0 );
    }
    return (void*)(vaddr + (Where&0xFFF));
}
#define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsMapMemory

void AcpiOsUnmapMemory (void *LogicalAddress, ACPI_SIZE Length) {
    k_vmem_free( (size_t)LogicalAddress );
}
#define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsUnmapMemory

#endif /* __ACOMAMORI_H__ */
