// acpi.cpp -- Non-OSL stuff related to ACPI
#include "includes.h"
#include "core/acpi.h"

void initialize_acpi() {
    ACPI_STATUS err = AE_OK;
    
    kprintf("acpi: early initialization\n");
    err = AcpiInitializeSubsystem();
    if(err != AE_OK) {
        kprintf("acpi: AcpiInitializeSubsystem failed with error code 0x%x: %s\n", err, const_cast<char*>(AcpiFormatException(err)));
        return;
    }
        
    kprintf("acpi: table initialization\n");
    err = AcpiInitializeTables(NULL, 16, TRUE);
    if(err != AE_OK) {
        kprintf("acpi: AcpiInitializeTables failed with error code 0x%x: %s\n", err, const_cast<char*>(AcpiFormatException(err)));
        return;
    }
    
    kprintf("acpi: loading tables\n");
    err = AcpiLoadTables();
    if(err != AE_OK) {
        kprintf("acpi: AcpiLoadTables failed with error code 0x%x: %s\n", err, const_cast<char*>(AcpiFormatException(err)));
        return;
    }
    
    kprintf("acpi: HW initialization\n");
    err = AcpiEnableSubsystem( ACPI_FULL_INITIALIZATION );
    if(err != AE_OK) {
        kprintf("acpi: AcpiEnableSubsystem failed with error code 0x%x: %s\n", err, const_cast<char*>(AcpiFormatException(err)));
        return;
    }
        
    kprintf("acpi: object initialization\n");
    err = AcpiInitializeObjects( ACPI_FULL_INITIALIZATION );
    if(err != AE_OK) {
        kprintf("acpi: AcpiInitializeObjects failed with error code 0x%x: %s\n", err, const_cast<char*>(AcpiFormatException(err)));
        return;
    }

    kprintf("acpi: basic ACPI initialization complete\n");
}
