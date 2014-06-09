// pci.cpp
// also handles PCI Express (well, whenever I get around to writing support for it)

#include "includes.h"
#include "arch/x86/sys.h"
#include "device/pci.h"
#include "device/pci_dev_info.h"
#include "lib/vector.h"

vector<pci_device*> pci_devices;

uint32_t pci_read_config_32(char bus, char device, char func, char offset) {
    uint32_t config_addr = (0x80000000 | (((uint32_t)bus)<<16) | (((uint32_t)device)<<11) | (((uint32_t)func)<<8) | (offset&0xFC));
    io_outd(PCI_IO_CONFIG_ADDRESS, config_addr);
    return io_ind(PCI_IO_CONFIG_DATA);
}

uint16_t pci_read_config_16(char bus, char device, char func, char offset) {
    uint32_t tmp = pci_read_config_32(bus, device, func, offset);
    tmp &= ( 0xFFFF << ( (offset&3)*8 ) );
    tmp >>= ( (offset&3)*8 );
    return tmp & 0xFFFF;
}

uint8_t pci_read_config_8(char bus, char device, char func, char offset) {
    uint32_t tmp = pci_read_config_32(bus, device, func, offset);
    tmp &= ( 0xFF << ( (offset&3)*8 ) );
    tmp >>= ( (offset&3)*8 );
    return tmp & 0xFF;
}

void pci_write_config_32(char bus, char device, char func, char offset, uint32_t data) {
    uint32_t config_addr = (0x80000000 | (((uint32_t)bus)<<16) | (((uint32_t)device)<<11) | (((uint32_t)func)<<8) | (offset&0xFC));
    io_outd(PCI_IO_CONFIG_ADDRESS, config_addr);
    return io_outd(PCI_IO_CONFIG_DATA, data);
}

void pci_write_config_16(char bus, char device, char func, char offset, uint16_t data) {
    uint32_t tmp = pci_read_config_32(bus, device, func, offset & 0xFC);
    tmp &= ~( 0xFFFF<<((offset&3)*8) );
    data <<= (offset&3)*8;
    tmp |= data;
    return pci_write_config_32(bus, device, func, offset, tmp);
}

void pci_write_config_8(char bus, char device, char func, char offset, uint8_t data) {
    uint32_t tmp = pci_read_config_32(bus, device, func, offset & 0xFC);
    tmp &= ~( 0xFF<<((offset&3)*8) );
    data <<= (offset&3)*8;
    tmp |= data;
    return pci_write_config_32(bus, device, func, offset, tmp);
}

void pci_register_function(char bus, char device, char func) {
    if( pci_read_config_16(bus, device, 0, 0) != 0xFFFF ) {
        pci_device *new_device = new pci_device;
        if( new_device == NULL )
            panic("pci: failed to allocate new device structure!");
        new_device->bus           = bus;
        new_device->device        = device;
        new_device->func          = func;
        new_device->prog_if       = pci_read_config_8( bus, device, func, 0x09 );
        new_device->subclass_code = pci_read_config_8( bus, device, func, 0x0A );
        new_device->class_code    = pci_read_config_8( bus, device, func, 0x0B );
        new_device->header_type   = pci_read_config_8(bus, device, func, 0x0E);
        new_device->vendorID      = pci_read_config_16(bus, device, func, 0);
        new_device->deviceID      = pci_read_config_16(bus, device, func, 2);
        pci_devices.add(new_device);
        char* ven_name = pci_get_ven_name(new_device->vendorID);
        char* dev_type = pci_get_dev_type( new_device->class_code, new_device->subclass_code, new_device->prog_if );
        //kprintf("pci: registered new device on b%u/d%u/f%u.\n", bus, device, func);
        //kprintf("pci: deviceID: 0x%x\n", new_device->deviceID);
        //kprintf("pci[%u/%u/%u]: vendorID: 0x%x - %s\n", bus, device, func, new_device->vendorID, ven_name);
        kprintf("pci[%u/%u/%u]: class code: 0x%x, subclass code: 0x%x, progIF: 0x%x\n", bus, device, func, new_device->class_code, new_device->subclass_code, new_device->prog_if);
        //kprintf("pci[%u/%u/%u]: device type: %s.\n", bus, device, func, dev_type);
    }
}

// checks whether a certain function is a PCI bridge and scans it if it is
void pci_check_bridge( char bus, char device, char func ) {
    uint8_t class_code = pci_read_config_8( bus, device, func, 0x0B );
    uint8_t subclass_code = pci_read_config_8( bus, device, func, 0x0A );
    
    if( (class_code == 0x06) && (subclass_code == 0x04) ) {
        uint8_t secondary_bus = pci_read_config_8( bus, device, func, 0x19 );
        pci_check_bus( secondary_bus );
    }
}

void pci_check_device( char bus, char device ) {
    uint16_t vendorID = pci_read_config_16(bus, device, 0, 0);
    if( vendorID != 0xFFFF ) {
        uint8_t headerType = pci_read_config_8(bus, device, 0, 0x0E);
        pci_register_function( bus, device, 0 );
        pci_check_bridge( bus, device, 0 );
        if( (headerType & 0x80) > 0 ) {
            for(char func=1;func<8;func++) {
                if( pci_read_config_16(bus, device, func, 0) != 0xFFFF ) {
                    pci_register_function( bus, device, func );
                    pci_check_bridge( bus, device, func );
                }
            }
        }
    }
}

void pci_check_bus( char bus ) {
    for( char device=0;device<32;device++ ) {
        pci_check_device( bus, device );
    }
}

void pci_check_all_buses() {
    uint8_t main_header_type = pci_read_config_8(0,0,0, 0x0E);
    if( (main_header_type & 0x80) == 0 ) {
        pci_check_bus(0);
    } else {
        for( char func=0;func<8;func++ ) {
            if( pci_read_config_16(0, 0, func, 0) != 0xFFFF ) {
                pci_check_bus( func );
            }
        }
    }
}

// just used for prettyprinting / logging output

char *pci_unknown_vendor = "Unknown Vendor";
char *pci_unknown_devtype = "Unknown Device Type";

char *pci_get_ven_name( uint16_t venID ) {
    for( unsigned int i=0;i<PCI_VENTABLE_LEN;i++ ) {
        if( PciVenTable[i].VenId == venID ) {
            return PciVenTable[i].VenFull;
        }
    }
    return pci_unknown_vendor;
}

char *pci_get_dev_type( uint8_t class_code, uint8_t subclass_code, uint8_t progif ) {
    char *ret = NULL;
    for( unsigned int i=0;i<PCI_CLASSCODETABLE_LEN;i++) {
        if( (PciClassCodeTable[i].BaseClass == class_code) && (PciClassCodeTable[i].SubClass == subclass_code) && (PciClassCodeTable[i].ProgIf == progif) ) {
            ret = concatentate_strings(PciClassCodeTable[i].BaseDesc, concatentate_strings( "-", concatentate_strings( PciClassCodeTable[i].SubDesc, concatentate_strings( "-", PciClassCodeTable[i].ProgDesc ) ) ) );
            return ret;
        }
    }
    return pci_unknown_devtype;
}