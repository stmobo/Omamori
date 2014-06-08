// acpi.cpp -- Non-OSL stuff related to ACPI
#include "includes.h"
#include "core/acpi.h"

void initialize_acpi() {
    ACPI_STATUS err = AE_OK;
    
    kprintf("acpi: early initialization\n");
    err = AcpiInitializeSubsystem();
    if(err != AE_OK)
        panic("acpi: AcpiInitializeSubsystem failed with error code 0x%x: %s\n", err, const_cast<char*>(AcpiFormatException(err)));
        
    kprintf("acpi: table initialization\n");
    err = AcpiInitializeTables(NULL, 16, TRUE);
    if(err != AE_OK)
        panic("acpi: AcpiInitializeTables failed with error code 0x%x: %s\n", err, const_cast<char*>(AcpiFormatException(err)));
    
    kprintf("acpi: loading tables\n");
    err = AcpiLoadTables();
    if(err != AE_OK)
        panic("acpi: AcpiLoadTables failed with error code 0x%x: %s\n", err, const_cast<char*>(AcpiFormatException(err)));
    
    kprintf("acpi: HW initialization\n");
    err = AcpiEnableSubsystem( ACPI_FULL_INITIALIZATION );
    if(err != AE_OK)
        panic("acpi: AcpiEnableSubsystem failed with error code 0x%x: %s\n", err, const_cast<char*>(AcpiFormatException(err)));
        
    kprintf("acpi: object initialization\n");
    err = AcpiInitializeObjects( ACPI_FULL_INITIALIZATION );
    if(err != AE_OK)
        panic("acpi: AcpiInitializeObjects failed with error code 0x%x: %s\n", err, const_cast<char*>(AcpiFormatException(err)));
}