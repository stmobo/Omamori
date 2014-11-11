// pci.cpp
// also handles PCI Express (well, whenever I get around to writing support for it)

#include "includes.h"
#include "arch/x86/sys.h"
#include "core/paging.h"
#include "device/pci.h"
#include "device/pci_dev_info.h"
#include "lib/vector.h"

vector<pci_device*> pci_devices;
// pages used to allocate MMIO ranges that are less than a page in size
vector<page_frame*> uncached_frames;
vector<size_t>      uncached_pages;
vector<uint32_t>    pci_mmio_unallocated;
// MMIO ranges are allocated in 128-byte blocks, giving us 32 blocks per page
// MMIO ranges are never released.

static void pci_mmio_range_add_block() {
    page_frame *new_frame = pageframe_allocate(1);
    size_t new_page = k_vmem_alloc(1);
    
    uncached_frames.add_end( new_frame );
    uncached_pages.add_end( new_page );
    pci_mmio_unallocated.add_end( 0 );
}

void pci_mmio_range_allocate( unsigned int length ) {
    if( (length % 128) > 0 ) {
        length += (128 - (length % 128));
    }
    
    unsigned int n_blocks = length / 128;
    uint32_t bitmask = 0;
    for(unsigned int i=0;i<n_blocks;i++) {
        bitmask <<= 1;
        bitmask |= 1;
    }
    
    unsigned int chosen_block;
    unsigned int block_offset;
    bool found = false;
    for(int i=0;i<pci_mmio_unallocated.count();i++) {
        if( pci_mmio_unallocated[i] != 0xFFFFFFFF ) {
            uint32_t tmp = bitmask;
            for(unsigned int j=0;j<(32-n_blocks);j++) {
                if( (pci_mmio_unallocated[i] & tmp) == 0 ) {
                    chosen_block = i;
                    pci_mmio_unallocated.set(i, tmp);
                    block_offset = j;
                    found = true;
                    break;
                }
                tmp <<= 1;
            }
            if( found )
                break;
        }
    }
    
    if( !found ) {
        pci_mmio_range_add_block();
        chosen_block = pci_mmio_unallocated.count();
        pci_mmio_unallocated.set(chosen_block, bitmask);
        block_offset = 0;
    }
    
    
}

uint32_t pci_read_config_32(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    uint32_t config_addr = (0x80000000 | (((uint32_t)bus)<<16) | (((uint32_t)device)<<11) | (((uint32_t)func)<<8) | (offset&0xFC));
    io_outd(PCI_IO_CONFIG_ADDRESS, config_addr);
    return io_ind(PCI_IO_CONFIG_DATA);
}

uint16_t pci_read_config_16(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    uint32_t tmp = pci_read_config_32(bus, device, func, offset);
    tmp &= ( 0xFFFF << ( (offset&3)*8 ) );
    tmp >>= ( (offset&3)*8 );
    return tmp & 0xFFFF;
}

uint8_t pci_read_config_8(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    uint32_t tmp = pci_read_config_32(bus, device, func, offset);
    tmp &= ( 0xFF << ( (offset&3)*8 ) );
    tmp >>= ( (offset&3)*8 );
    return tmp & 0xFF;
}

void pci_write_config_32(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, uint32_t data) {
    uint32_t config_addr = (0x80000000 | (((uint32_t)bus)<<16) | (((uint32_t)device)<<11) | (((uint32_t)func)<<8) | (offset&0xFC));
    io_outd(PCI_IO_CONFIG_ADDRESS, config_addr);
    return io_outd(PCI_IO_CONFIG_DATA, data);
}

void pci_write_config_16(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, uint16_t data) {
    uint32_t tmp = pci_read_config_32(bus, device, func, offset & 0xFC);
    tmp &= ~( (uint32_t)0xFFFF<<((offset&3)*8) );
    //data <<= (offset&3)*8;
    tmp |= (uint32_t)(((uint32_t)data) << ((offset&3)*8));
    return pci_write_config_32(bus, device, func, offset, tmp);
}

void pci_write_config_8(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, uint8_t data) {
    uint32_t tmp = pci_read_config_32(bus, device, func, offset & 0xFC);
    tmp &= ~( (uint32_t)0xFF<<((offset&3)*8) );
    //data <<= (offset&3)*8;
    tmp |= (uint32_t)(((uint32_t)data) << ((offset&3)*8));
    return pci_write_config_32(bus, device, func, offset, tmp);
}

void pci_register_function(uint8_t bus, uint8_t device, uint8_t func) {
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
        // read BARs:
        for(int i=0;i<6;i++) {
            uint32_t bar = new_device->registers[i].raw = pci_read_config_32(bus, device, func, 0x10 + (i*4));
            new_device->registers[i].io_space  = (bar & 1);
            if( bar & 1 ) {
                new_device->registers[i].phys_address = bar & 0xFFFFFFFC;
                new_device->registers[i].virt_address = new_device->registers[i].phys_address;
            } else {
                new_device->registers[i].phys_address = bar & 0xFFFFFFF0;
            }
            new_device->registers[i].loc_area  = (bar & 6) >> 1;
            new_device->registers[i].cacheable = (bar & 8);
            
            // determine zero-forced bits
            pci_write_config_32(bus, device, func, 0x10 + (i*4), 0xFFFFFFFF);
            uint32_t n =  pci_read_config_32(bus, device, func, 0x10 + (i*4));
            uint32_t tmp = (n >> 4); // shift out RO bits
            tmp = ~tmp & 0x0FFFFFFF;   // find out what's been set to 1
            tmp++;                     // now all of the masked bits should be set, so add one to bring it to a power of two
            
            new_device->registers[i].size      = tmp;
            pci_write_config_32(bus, device, func, 0x10 + (i*4), bar);
        }
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
void pci_check_bridge( uint8_t bus, uint8_t device, uint8_t func ) {
    uint8_t class_code = pci_read_config_8( bus, device, func, 0x0B );
    uint8_t subclass_code = pci_read_config_8( bus, device, func, 0x0A );
    
    if( (class_code == 0x06) && (subclass_code == 0x04) ) {
        uint8_t secondary_bus = pci_read_config_8( bus, device, func, 0x19 );
        pci_check_bus( secondary_bus );
    }
}

void pci_check_device( uint8_t bus, uint8_t device ) {
    uint16_t vendorID = pci_read_config_16(bus, device, 0, 0);
    if( vendorID != 0xFFFF ) {
        uint8_t headerType = pci_read_config_8(bus, device, 0, 0x0E);
        pci_register_function( bus, device, 0 );
        pci_check_bridge( bus, device, 0 );
        if( (headerType & 0x80) > 0 ) {
            for(uint8_t func=1;func<8;func++) {
                if( pci_read_config_16(bus, device, func, 0) != 0xFFFF ) {
                    pci_register_function( bus, device, func );
                    pci_check_bridge( bus, device, func );
                }
            }
        }
    }
}

void pci_check_bus( uint8_t bus ) {
    for( uint8_t device=0;device<32;device++ ) {
        pci_check_device( bus, device );
    }
}

void pci_check_all_buses() {
    uint8_t main_header_type = pci_read_config_8(0,0,0, 0x0E);
    if( (main_header_type & 0x80) == 0 ) {
        pci_check_bus(0);
    } else {
        for( uint8_t func=0;func<8;func++ ) {
            if( pci_read_config_16(0, 0, func, 0) != 0xFFFF ) {
                pci_check_bus( func );
            }
        }
    }
}

void pci_bar::writeb( uint32_t offset, uint8_t data ) {
    if( this->io_space ) {
        io_outb( ((uint16_t)this->phys_address)+offset, data );
    } else {
        uint8_t *ptr = (uint8_t*)this->virt_address;
        ptr[offset] = data;
    }
}

void pci_bar::writew( uint32_t offset, uint16_t data ) {
    if( this->io_space ) {
        io_outb( ((uint16_t)this->phys_address)+offset, data );
    } else {
        uint16_t *ptr = (uint16_t*)this->virt_address;
        ptr[offset] = data;
    }
}

void pci_bar::writed( uint32_t offset, uint32_t data ) {
    if( this->io_space ) {
        io_outd( ((uint16_t)this->phys_address)+offset, data );
    } else {
        uint32_t *ptr = (uint32_t*)this->virt_address;
        ptr[offset] = data;
    }
}

uint8_t pci_bar::readb( uint32_t offset ) {
    if( this->io_space ) {
        return io_inb( ((uint16_t)this->phys_address)+offset );
    } else {
        uint8_t *ptr = (uint8_t*)this->virt_address;
        return ptr[offset];
    }
}

uint16_t pci_bar::readw( uint32_t offset ) {
    if( this->io_space ) {
        return io_inw( ((uint16_t)this->phys_address)+offset );
    } else {
        uint16_t *ptr = (uint16_t*)this->virt_address;
        return ptr[offset];
    }
}

uint32_t pci_bar::readd( uint32_t offset ) {
    if( this->io_space ) {
        return io_ind( ((uint16_t)this->phys_address)+offset );
    } else {
        uint32_t *ptr = (uint32_t*)this->virt_address;
        return ptr[offset];
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