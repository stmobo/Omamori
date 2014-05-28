// acpica_osl.cpp - compat layer for ACPICA

#include "includes.h"
extern "C" {
    #include "acpica/acconfig.h"
    #include "acpica/acmacros.h"
    #include "acpica/platform/acenv.h"
    #include "acpica/actypes.h"
    #include "acpica/acexcep.h"
    #include "acpica/acrestyp.h"
    #include "acpica/acoutput.h"
    #include "acpica/acpixf.h"    
    #include "acpica/actbl.h"
    #include "acpica/aclocal.h"
    #include "acpica/acobject.h"
    #include "acpica/acstruct.h"
    #include "acpica/acglobal.h"
    #include "acpica/achware.h"
    #include "acpica/acutils.h"
}
#include "arch/x86/irq.h"
#include "core/paging.h"
#include "core/scheduler.h"
#include "device/pit.h"
#include "device/pci.h"
#include "device/vga.h"
#include "lib/sync.h"

struct rdsp_descriptor {
    char     sig[8];
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rdst_addr;
};

// ACPICA is written in C.
extern "C" {
    char rdsp_magic[8] = { 'R', 'S', 'D', ' ', 'P', 'T', 'R', ' ' }; // no null terminator

    /*
     * Memory allocation/deallocation
     */
     
    ACPI_STATUS AcpiOsInitialize() {
        return AE_OK;
    }
    
    ACPI_STATUS AcpiOsTerminate() {
        return AE_OK;
    }
    
    ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer()
    {
        ACPI_SIZE Ret = 0;
        size_t search_addr = 0x1000;
        //AcpiFindRootPointer(&Ret);
        // Search the entire lower 4MB of memory.
        // except for the first 4KB.
        while(true) {
            char *data = (char*)search_addr;
            if( data[0] == rdsp_magic[0] ) {
                bool found = true;
                for( int i=1;i<8;i++ ) {
                    if( data[i] != rdsp_magic[i] ) {
                        found = false;
                        break;
                    }
                }
                if(found) {
                    rdsp_descriptor *rdsp = (rdsp_descriptor*)search_addr;
                    // calculate checksum
                    uint8_t checksum = rdsp->checksum;
                    for(int i=0;i<8;i++) {
                        checksum += rdsp->sig[i];
                    }
                    for(int i=0;i<6;i++) {
                        checksum += rdsp->oem_id[i];
                    }
                    checksum += rdsp->revision;
                    checksum += (rdsp->rdst_addr)         & 0xFF;
                    checksum += ((rdsp->rdst_addr) >> 8)  & 0xFF;
                    checksum += ((rdsp->rdst_addr) >> 16) & 0xFF;
                    checksum += ((rdsp->rdst_addr) >> 24) & 0xFF;
                    if(checksum == 0) {
                        kprintf("acpi: found RDSP at 0x%x\n", (unsigned long long int)search_addr);
                        char oem_id[7];
                        for(int i=0;i<6;i++) {
                            oem_id[i] = rdsp->oem_id[i];
                        }
                        oem_id[6] = '\0';
                        kprintf("acpi: oem_id: %s\n", (uint64_t)oem_id);
                        kprintf("acpi: revision: 0x%x\n", (uint64_t)rdsp->revision);
                        kprintf("acpi: rdst_addr: 0x%x\n",(uint64_t)rdsp->rdst_addr);
                        kprintf("acpi: checksum: 0x%x\n", (uint64_t)rdsp->checksum);
                        return search_addr;
                    }
                }
            }
            search_addr += 0x8;
        }
        kprintf("acpi: could not find RDSP!\n");
        return 0xFFFFFFFF;
    }
    
    ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *PredefinedObject, ACPI_STRING *NewValue) {
        *NewValue = NULL;
        return AE_OK;
    }
    
    ACPI_STATUS AcpiOsPhysicalTableOverride ( void *ExistingTable, ACPI_PHYSICAL_ADDRESS *NewAddress, UINT32 *NewTableLength) {
        *NewAddress = NULL;
        return AE_OK;
    }
    
    // TODO: define ACPI_TABLE_HEADER as an actual structure
    ACPI_STATUS AcpiOsTableOverride(void *ExistingTable, void **NewTable) {
        *NewTable = NULL;
        return AE_OK;
    }
     
    void *AcpiOsAllocate ( ACPI_SIZE Size ) {
        return kmalloc(Size);
    }
    #define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsAllocate
    
    void AcpiOsFree (void *Memory) {
        return kfree(Memory);
    }
    #define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsFree

    void *AcpiOsMapMemory ( ACPI_PHYSICAL_ADDRESS Where, ACPI_SIZE Length) {
        if( Length & 0xFFF ) {
            Length += 0x1000;
        }
        Length &= 0xFFFFF000;
        size_t offset = Where & 0xFFF;
        int n_frames = Length / 0x1000;
        size_t vaddr = paging_map_phys_address( Where, n_frames );
        return (void*)(vaddr + offset);
    }
    #define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsMapMemory

    void AcpiOsUnmapMemory (void *LogicalAddress, ACPI_SIZE Length) {
        if( Length & 0xFFF ) {
            Length += 0x1000;
        }
        Length &= 0xFFFFF000;
        int n_frames = Length / 0x1000;
        size_t vaddr_page = ((size_t)LogicalAddress) & 0xFFFFF000;
        paging_unmap_phys_address( vaddr_page, n_frames );
    }
    #define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsUnmapMemory
    
    BOOLEAN AcpiOsReadable(void *Memory, ACPI_SIZE Length) {
        if( Length & 0xFFF ) {
            Length += 0x1000;
        }
        Length &= 0xFFFFF000;
        int n_pages = Length / 0x1000;
        for(int i=0;i<n_pages;i++) {
            uint32_t pte = paging_get_pte( ((size_t)Memory)+(i*0x1000) );
            if(pte == 0) {
                return FALSE;
            }
        }
        return TRUE;
    }
    
    BOOLEAN AcpiOsWritable(void *Memory, ACPI_SIZE Length) {
        if( Length & 0xFFF ) {
            Length += 0x1000;
        }
        Length &= 0xFFFFF000;
        int n_pages = Length / 0x1000;
        for(int i=0;i<n_pages;i++) {
            uint32_t pte = paging_get_pte( ((size_t)Memory)+(i*0x1000) );
            if(pte == 0) {
                return FALSE;
            }
        }
        return TRUE;
    }
    
    /*
     * Spinlocks
     */
    
    ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK *OutHandle) {
        if( OutHandle == NULL )
            return AE_BAD_PARAMETER;
        spinlock *lock = new spinlock;
        if( lock != NULL ) {
            (*OutHandle) = (void*)lock;
            return AE_OK;
        }
        return AE_NO_MEMORY;
    }
    #define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsCreateLock

    ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK Handle) {
        if( Handle == NULL )
            return 0;
        //kprintf("acpi_osl: Acquiring spinlock!");
        spinlock *lock = (spinlock*)Handle;
        lock->lock_cli();
        return 0;
    }
    #define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsAcquireLock

    void AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS Flags) {
        if( Handle == NULL )
            return;
        //kprintf("acpi_osl: Releasing spinlock!");
        spinlock *lock = (spinlock*)Handle;
        lock->unlock_cli();
        return;
    }
    #define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsReleaseLock

    void AcpiOsDeleteLock(ACPI_SPINLOCK Handle) {
        if( Handle == NULL )
            return;
        spinlock *lock = (spinlock*)Handle;
        delete lock;
        return;
    }
    #define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsDeleteLock
    
    /*
     * Mutexes
     */
    
    ACPI_STATUS AcpiOsCreateMutex(ACPI_MUTEX *OutHandle) {
        if( OutHandle == NULL )
            return AE_BAD_PARAMETER;
        mutex *lock = new mutex;
        if( lock != NULL ) {
            (*OutHandle) = (void*)lock;
            return AE_OK;
        }
        return AE_NO_MEMORY;
    }
    
    void AcpiOsDeleteMutex(ACPI_MUTEX Handle) {
        if( Handle == NULL )
            return;
        mutex *lock = (mutex*)Handle;
        delete lock;
        return;
    }
    
    ACPI_STATUS AcpiOsAcquireMutex(ACPI_MUTEX Handle, UINT16 Timeout) {
        if( Handle == NULL )
            return AE_BAD_PARAMETER;
        //kprintf("acpi_osl: Acquiring mutex!");
        mutex *lock = (mutex*)Handle;
        uint64_t sys_time = get_sys_time_counter();
        if( Timeout == 0 ) {
            if( lock->trylock() )
                return AE_OK;
            return AE_TIME;
        }
        if( Timeout == 0xFFFF ) {
            lock->lock();
            return AE_OK;
        }
        while(true) {
            if( lock->trylock() )
                return AE_OK;
            process_switch_immediate();
            if( get_sys_time_counter() - sys_time >= Timeout ) {
                return AE_TIME;
            }
        }
        return AE_OK;
    }
    
    void AcpiOsReleaseMutex(ACPI_MUTEX Handle) {
        if( Handle == NULL )
            return;
        //kprintf("acpi_osl: Unlocking mutex!");
        mutex *lock = (mutex*)Handle;
        lock->unlock();
        return;
    }
    
    /*
     * Semaphores
     */
     
    ACPI_STATUS AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits, ACPI_SEMAPHORE *OutHandle) {
        if( OutHandle == NULL )
            return AE_BAD_PARAMETER;
        semaphore *sema = new semaphore(InitialUnits, MaxUnits);
        if( sema != NULL ) {
            (*OutHandle) = (void*)sema;
            return AE_OK;
        }
        return AE_NO_MEMORY;
    }
     
    ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle) {
        if( Handle == NULL )
            return AE_BAD_PARAMETER;
        semaphore *sema =(semaphore*)Handle;
        delete sema;
        return AE_OK;
    }
     
    ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units, UINT16 Timeout) {
        if( Handle == NULL )
            return AE_BAD_PARAMETER;
        //kprintf("acpi_osl: Waiting for semaphore!\n");
        semaphore *lock =(semaphore*)Handle;
        uint64_t sys_time = get_sys_time_counter();
        if( Timeout == 0 ) {
            if( lock->try_acquire( Units ) )
                return AE_OK;
            return AE_TIME;
        }
        if( Timeout == 0xFFFF ) {
            lock->try_acquire( Units );
            return AE_OK;
        }
        while(true) {
            if( lock->try_acquire( Units ) )
                return AE_OK;
            process_switch_immediate();
            if( get_sys_time_counter() - sys_time >= Timeout ) {
                return AE_TIME;
            }
        }
        return AE_OK;
    }
     
    ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units) {
        if( Handle == NULL )
            return AE_BAD_PARAMETER;
        //kprintf("acpi_osl: Signaling semaphore!\n");
        semaphore *sema =(semaphore*)Handle;
        if( sema->release( Units ) )
            return AE_OK;
        return AE_LIMIT;
    }
     
    /*
     * Interrupt handling
     */

    ACPI_OSD_HANDLER __acpi_irq;
    void* __acpi_irq_context;
    void __acpi_interrupt() {
        __acpi_irq(__acpi_irq_context);
    }
      
    ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 InterruptLevel, ACPI_OSD_HANDLER Handler, void *Context) {
        __acpi_irq = Handler;
        __acpi_irq_context = Context;
        irq_add_handler(InterruptLevel, (size_t)&__acpi_interrupt);
        return AE_OK;
    }
    
    ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER Handler) {
        irq_remove_handler(InterruptNumber);
        return AE_OK;
    }
    
    
    /*
     * Multitasking
     */
     
    process **acpi_threads = NULL;
    int n_acpi_threads = 0;
    ACPI_THREAD_ID AcpiOsGetThreadId() {
        return process_current->id;
    }
    
    ACPI_STATUS AcpiOsExecute(uint32_t Type, void *Function, void *Context) {
        if( Function == NULL )
            return AE_BAD_PARAMETER;
        process *proc = new process( (size_t)Function, false, 1, concatentate_strings("acpi_process", int_to_decimal(n_acpi_threads)), Context, 1 );
        if( proc ) {
            spawn_process( proc );
            acpi_threads = extend<process*>(acpi_threads, n_acpi_threads);
            acpi_threads[n_acpi_threads] = proc;
            n_acpi_threads++;
            return AE_OK;
        }
        return AE_ERROR;
    }
    
    void AcpiOsSleep(UINT64 Milliseconds) {
        unsigned long long int current = get_sys_time_counter();
        while( (get_sys_time_counter() - current) <= Milliseconds ) {
            asm volatile("hlt" : : : "memory");
        }
    }
    
    void AcpiOsStall(UINT32 Microseconds) {
        int n_io_waits = (Microseconds / 60)+1;
        for( int i=0;i<=n_io_waits;i++ ) {
            io_wait();
        }
        return;
    }
    
    void AcpiOsWaitEventsComplete() {
        bool all_processes_dead = false;
        while(!all_processes_dead) {
            all_processes_dead = true;
            for(int i=0;i<n_acpi_threads;i++) {
                if(acpi_threads[i]->state != process_state::dead) {
                    all_processes_dead = false;
                    break;
                }
            }
            if( all_processes_dead )
                return;
            process_switch_immediate();
        }
    }
    
    /*
     * printf / printf_varg
     */
    
    void AcpiOsVprintf(const char* format, va_list args) {
        kprintf_varg(format, args);
    }
    
    void AcpiOsPrintf(const char* format, ...) {
        va_list args;
        va_start(args, format);
        kprintf_varg(format, args);
        va_end(args);
    }
    
    /*
     * Physical memory reading/writing
     */
    
    ACPI_STATUS AcpiOsReadMemory( ACPI_PHYSICAL_ADDRESS Address,  UINT64 *Value, UINT32 Width ) {
        // Map that address into memory, read the specified address, and finally unmap it.
        size_t fourk_boundary = Address & 0xFFFFF000;
        size_t vaddr = k_vmem_alloc( 1 );
        paging_set_pte( vaddr, fourk_boundary, 0 );
        switch(Width) {
            case 8:
                {
                    uint8_t *data = (uint8_t*)vaddr;
                    *Value = (uint64_t)*data;
                }
            case 16:
                {
                    uint16_t *data = (uint16_t*)vaddr;
                    *Value = (uint64_t)*data;
                }
            case 32:
                {
                    uint32_t *data = (uint32_t*)vaddr;
                    *Value = (uint64_t)*data;
                }
            case 64:
                {
                    uint64_t *data = (uint64_t*)vaddr;
                    *Value = *data;
                }
        }
        k_vmem_free( vaddr );
        return AE_OK;
    }
    
    ACPI_STATUS AcpiOsWriteMemory( ACPI_PHYSICAL_ADDRESS Address,  UINT64 Value, UINT32 Width ) {
        // Map that address into memory, read the specified address, and finally unmap it.
        size_t fourk_boundary = Address & 0xFFFFF000;
        size_t vaddr = k_vmem_alloc( 1 );
        paging_set_pte( vaddr, fourk_boundary, 0 );
        switch(Width) {
            case 8:
                {
                    uint8_t *data = (uint8_t*)vaddr;
                    *data = (uint8_t)Value;
                }
            case 16:
                {
                    uint16_t *data = (uint16_t*)vaddr;
                    *data = (uint16_t)Value;
                }
            case 32:
                {
                    uint32_t *data = (uint32_t*)vaddr;
                    *data = (uint32_t)Value;
                }
            case 64:
                {
                    uint64_t *data = (uint64_t*)vaddr;
                    *data = Value;
                }
        }
        k_vmem_free( vaddr );
        return AE_OK;
    }
    
    /*
     * Port-based I/O
     */
    
    ACPI_STATUS AcpiOsReadPort( ACPI_IO_ADDRESS Port, UINT32 *Value, UINT32 Width ) {
        switch(Width) {
            case 8:
                {
                    *((uint8_t*)Value) = io_inb(Port);
                }
            case 16:
                {
                    *((uint16_t*)Value) = io_inw(Port);
                }
            case 32:
                {
                    *((uint32_t*)Value) = io_ind(Port);
                }
        }
        return AE_OK;
    }
    
    ACPI_STATUS AcpiOsWritePort( ACPI_IO_ADDRESS Port, UINT32 Value, UINT32 Width ) {
        switch(Width) {
            case 8:
                {
                    io_outb( Port, (uint8_t)Value );
                }
            case 16:
                {
                    io_outw( Port, (uint16_t)Value );
                }
            case 32:
                {
                    io_outd( Port, (uint32_t)Value );
                }
        }
        return AE_OK;
    }
    
    /*
     * Debugger signaling
     */
    
    ACPI_STATUS AcpiOsSignal( UINT32 Function, void *Info ) {
        return AE_OK;
    }
    
    UINT64 AcpiOsGetTimer() {
        return rdtsc();
    }
    
    /*
     * PCI configuration
     */
     
    ACPI_STATUS AcpiOsReadPciConfiguration( ACPI_PCI_ID PciId, UINT32 Register, UINT64 *Value, UINT32 Width) {
        uint32_t config_space_offset = (0x80000000 | (PciId.Bus<<16) | (PciId.Device<<11) | (PciId.Function<<8)) + (Register << 2);
        io_outw(PCI_IO_CONFIG_ADDRESS, config_space_offset);
        switch(Width) {
            case 8:
            case 16:
            case 32:
                    *Value = (uint64_t)(io_ind(PCI_IO_CONFIG_DATA) & ((1<<Width)-1));
            case 64:
                {
                    uint32_t d1 = io_inw(PCI_IO_CONFIG_DATA);
                    config_space_offset += 4;
                    io_outw(PCI_IO_CONFIG_ADDRESS, config_space_offset);
                    uint32_t d2 = io_inw(PCI_IO_CONFIG_DATA);
                    *Value = ( (((uint64_t)d2) << 32) | d1 );
                }
        }
        return AE_OK;
    }
    
    ACPI_STATUS AcpiOsWritePciConfiguration( ACPI_PCI_ID PciId, UINT32 Register, UINT64 Value, UINT32 Width) {
        uint32_t config_space_offset = (0x80000000 | (PciId.Bus<<16) | (PciId.Device<<11) | (PciId.Function<<8)) + (Register << 2);
        io_outw(PCI_IO_CONFIG_ADDRESS, config_space_offset);
        switch(Width) {
            case 8:
                 io_outb(PCI_IO_CONFIG_DATA, (uint8_t)(Value & 0x00000000000000FF));
            case 16:
                io_outw(PCI_IO_CONFIG_DATA, (uint16_t)(Value & 0x000000000000FFFF));
            case 32:
                io_outd(PCI_IO_CONFIG_DATA, (uint32_t)(Value & 0x00000000FFFFFFFF));
            case 64:
                {
                    // Output first dword
                    io_outd(PCI_IO_CONFIG_DATA, (uint32_t)(Value & 0x00000000FFFFFFFF));
                    config_space_offset += 4;
                    // Output second dword
                    io_outw(PCI_IO_CONFIG_ADDRESS, config_space_offset);
                    io_outd(PCI_IO_CONFIG_DATA, (uint32_t)((Value>>32) & 0x00000000FFFFFFFF));
                }
        }
        return AE_OK;
    }
}