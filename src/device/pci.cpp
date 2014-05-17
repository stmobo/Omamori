// pci.cpp

#include "includes.h"
#include "device/pci.h"

pci_device **pci_devices = NULL;
int pci_n_devices = 0;

uint32_t pci_read_config_32(char bus, char device, char func, char offset) {
    uint32_t config_addr = (0x80000000 | (bus<<16) | (device<<11) | (func<<8) | (offset&0xFC));
    io_outw(PCI_IO_CONFIG_ADDRESS, config_addr);
    return io_inw(PCI_IO_CONFIG_DATA);
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
    uint32_t config_addr = (0x80000000 | (bus<<16) | (device<<11) | (func<<8) | (offset&0xFC));
    io_outw(PCI_IO_CONFIG_ADDRESS, config_addr);
    return io_outw(PCI_IO_CONFIG_DATA, data);
}

void pci_write_config_16(char bus, char device, char func, char offset, uint16_t data) {
    uint32_t tmp = data;
    tmp <<= (offset&3)*8;
    return pci_write_config_32(bus, device, func, offset, tmp);
}

void pci_write_config_8(char bus, char device, char func, char offset, uint8_t data) {
    uint32_t tmp = data;
    tmp <<= (offset&3)*8;
    return pci_write_config_32(bus, device, func, offset, tmp);
}

char pci_check_device(char bus, char device, char func) {
    if( pci_read_config_16(bus, device, func, 0) == 0xFFFF )
        return 0;
    uint8_t header_type = pci_read_config_8(bus, device, func, 0x0E);
    if(header_type&0x80) {
        return 2;
    }
    return 1;
}

void pci_register_device(char bus, char device, char func) {
    if( pci_devices != NULL )  {
        pci_device **new_pci_devices = new pci_device*[pci_n_devices+1];
        for(int i=0;i<pci_n_devices;i++) {
            new_pci_devices[i] = pci_devices[i];
        }
        delete[] pci_devices;
        pci_devices = new_pci_devices;
    }
    if( pci_check_device(bus, device, func)  ) {
//#ifdef PCI_DEBUG
        kprintf("pci: registered device - bus %u - device %u - func %u\n", (long long int)bus, (long long int)device, (long long int)func);
        kprintf("pci: class code: 0x%x, subclass code: 0x%x\n", (long long int)pci_read_config_8( bus, device, 0, 0x0B ), (long long int)pci_read_config_8( bus, device, 0, 0x0A ));
        kprintf("pci: vendorID: 0x%x\n", (long long int)pci_read_config_16(bus, device, func, 0));
        kprintf("pci: deviceID: 0x%x\n", (long long int)pci_read_config_16(bus, device, func, 2));
//#endif
        pci_devices[pci_n_devices]->bus = bus;
        pci_devices[pci_n_devices]->device = device;
        pci_devices[pci_n_devices]->func = func;
        pci_devices[pci_n_devices]->class_code = pci_read_config_8( bus, device, func, 0x0B );
        pci_devices[pci_n_devices]->subclass_code = pci_read_config_8( bus, device, func, 0x0A );
        pci_devices[pci_n_devices]->header_type = pci_read_config_8(bus, device, func, 0x0E);
        pci_devices[pci_n_devices]->vendorID = pci_read_config_16(bus, device, func, 0);
        pci_devices[pci_n_devices]->deviceID = pci_read_config_16(bus, device, func, 2);
        pci_n_devices++;
    }
}

void pci_check_bus(char bus) {
    kprintf("pci: checking for devices on bus %u...\n", (long long int)bus);
    for(char device=0;device<32;device++) {
        if( pci_check_device(bus, device, 0) ) {
            uint8_t class_code = pci_read_config_8( bus, device, 0, 0x0B );
            uint8_t subclass_code = pci_read_config_8( bus, device, 0, 0x0A );
            uint8_t header_type = pci_read_config_8(bus, device, 0, 0x0E);
            kprintf("pci: found device - bus %u - device %u\n", bus, device);
            kprintf("pci: vendorID: 0x%x\n", (long long int)pci_read_config_16(bus, device, 0, 0));
            kprintf("pci: deviceID: 0x%x\n", (long long int)pci_read_config_16(bus, device, 0, 2));
            if( (class_code == 0x06) && (subclass_code == 0x04) ) {
                if( header_type&0x80 ) {
                    for(char func=0;func<8;func++) {
                        if( pci_check_device(bus, device, func) ) {
                            uint8_t other_end = pci_read_config_8(bus, device, func, 0x19);
//#ifdef PCI_DEBUG
                            kprintf("pci: found pci->pci bridge, bus %u -> %u\n", (long long int)bus, (long long int)other_end);
//#endif
                            pci_check_bus(other_end);
                        }
                    }
                } else {
                    uint8_t other_end = pci_read_config_8(bus, device, 0, 0x19);
//#ifdef PCI_DEBUG
                    kprintf("pci: found pci->pci bridge, bus %u -> %u\n", (long long int)bus, (long long int)other_end);
//#endif
                    pci_check_bus(other_end);
                }
            } else {
                for(char func=0;func<8;func++) {
                    pci_register_device(bus, device, func);
                }
            }
        }
    }
}