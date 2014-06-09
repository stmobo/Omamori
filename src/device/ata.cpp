// ata.cpp -- ATA Storage driver

#include "includes.h"
#include "device/pci.h"

// I assume that there's only one ATA controller in a system.
static struct __ata_controller {
    char bus    = 0;
    char device = 0;
    char func   = 0;
    char progif = 0;
    
    short primary_channel   = 0;
    short primary_control   = 0;
    short secondary_channel = 0;
    short secondary_control = 0;
    short dma_control_base  = 0;
    
    bool ready;
    
    void initialize( uint32_t, uint32_t, uint32_t, uint32_t, uint32_t );
} ata_controller;

static struct __ata_channel {
    short    base;
    short    control;
    short    bus_master;
    bool     interrupt;
    uint8_t  selected_drive;
    uint32_t prdt_phys;
    uint32_t prdt_virt;
    
    void initialize( short, short );
    void select( uint8_t );
} ata_channels[2];

void __ata_channel::initialize( short base, short control ) {
    this->base = base;
    this->control = control;
    page_frame *frames = pageframe_allocate(4); // I'm relying on the fact that buddy allocators always allocate contigous memory
    this->prdt_virt = k_vmem_alloc(4); 
    for(int i=0;i<4;i++) {
        if( (i > 0) && (frames[i].address != (frames[i-1].address+0x1000)) ) {
            panic("ata: did not allocate contigous frames for PRDT!\n");
        }
        paging_set_pte( prdt_virt+(i*0x1000), frames[i].address, 0 );
    }
    this->prdt_phys = frames[0].address;
}

void __ata_controller::initialize( uint32_t bar0, uint32_t bar1, uint32_t bar2, uint32_t bar3 ) {
    this->primary_channel   = bar0;
    this->primary_control   = bar1;
    this->secondary_channel = bar2;
    this->secondary_control = bar3;
    this->dma_control_base  = bar4;
    
    ata_channels[0].initialize( bar0, bar1 );
    ata_channels[1].initialize( bar2, bar3 );
    
    pci_write_config_32( this->bus, this->device, this->func, 0x10, bar0 );
    pci_write_config_32( this->bus, this->device, this->func, 0x14, bar1 );
    pci_write_config_32( this->bus, this->device, this->func, 0x18, bar2 );
    pci_write_config_32( this->bus, this->device, this->func, 0x1C, bar3 );
    pci_write_config_32( this->bus, this->device, this->func, 0x20, ATA_BUS_MASTER_START );
    
    // set PRDT addresses
    io_outb( ATA_BUS_MASTER_START+4, ( ata_channels[0].prdt_phys & 0xFF ) );
    io_outb( ATA_BUS_MASTER_START+5, ( (ata_channels[0].prdt_phys>>8) & 0xFF ) );
    io_outb( ATA_BUS_MASTER_START+6, ( (ata_channels[0].prdt_phys>>16) & 0xFF ) );
    io_outb( ATA_BUS_MASTER_START+7, ( (ata_channels[0].prdt_phys>>24) & 0xFF ) );
    
    io_outb( ATA_BUS_MASTER_START+0xC, ( ata_channels[1].prdt_phys & 0xFF ) );
    io_outb( ATA_BUS_MASTER_START+0xD, ( (ata_channels[1].prdt_phys>>8) & 0xFF ) );
    io_outb( ATA_BUS_MASTER_START+0xE, ( (ata_channels[1].prdt_phys>>16) & 0xFF ) );
    io_outb( ATA_BUS_MASTER_START+0xF, ( (ata_channels[1].prdt_phys>>24) & 0xFF ) );
}

void ata_write(uint8_t channel, uint8_t reg, uint8_t data) {
   if (reg > 0x07 && reg < 0x0C)
      ata_write(channel, ATA_REG_CONTROL, 0x80 | ata_channels[channel].interrupt ? 0 : 1);
   if (reg < 0x08)
      io_outb(ata_channels[channel].base  + reg - 0x00, data);
   else if (reg < 0x0C)
      io_outb(ata_channels[channel].base  + reg - 0x06, data);
   else if (reg < 0x0E)
      io_outb(ata_channels[channel].ctrl  + reg - 0x0A, data);
   else if (reg < 0x16)
      io_outb(ata_channels[channel].bmata + reg - 0x0E, data);
   if (reg > 0x07 && reg < 0x0C)
      ata_write(channel, ATA_REG_CONTROL, ata_channels[channel].interrupt ? 0 : 1);
}

uint8_t ata_read(uint8_t channel, uint8_t reg) {
   uint8_t result;
   if (reg > 0x07 && reg < 0x0C)
      ata_write(channel, ATA_REG_CONTROL, 0x80 | ata_channels[channel].interrupt ? 0 : 1);
   if (reg < 0x08)
      result = io_inb(ata_channels[channel].base + reg - 0x00);
   else if (reg < 0x0C)
      result = io_inb(ata_channels[channel].base  + reg - 0x06);
   else if (reg < 0x0E)
      result = io_inb(ata_channels[channel].ctrl  + reg - 0x0A);
   else if (reg < 0x16)
      result = io_inb(ata_channels[channel].bmata + reg - 0x0E);
   if (reg > 0x07 && reg < 0x0C)
      ata_write(channel, ATA_REG_CONTROL, ata_channels[channel].interrupt ? 0 : 1);
   return result;
}

void __ata_channel::select( uint8_t select_val ) {
    this->selected_drive = select_val;
    
    // read status 4 times (giving it time to switch)
    for(int i=0;i<4;i++)
        io_inb( this->base + 7 );
}

void ata_initialize() {
    for(unsigned int i=0;i<pci_devices.count();i++) {
        pci_device *current = pci_devices[i];
        if( (current->class_code == 0x01) && (current->subclass_code == 0x01) ) {
            kprintf("ata: found controller (device ID: %u [%u/%u/%u])\n", i, current->bus, current->device, current->func);
            ata_controller.bus    = current->bus;
            ata_controller.device = current->device;
            ata_controller.func   = current->func;
            ata_controller.progif = current->prog_if;
            
            ata_controller.initialize(0x1F0, 0x3F6, 0x170, 0x376);
            
            uint8_t test = ata_read(0, ATA_REG_STATUS);
            if( test == 0xFF ) {
                // floating bus, there's nothing here
                kprintf("ata: floating bus detected\n");
                return;
            }
            ata_controller.ready = true;
        }
    }
}