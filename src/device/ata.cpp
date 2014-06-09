// ata.cpp -- ATA Storage driver

#include "includes.h"
#include "arch/x86/sys.h"
#include "arch/x86/irq.h"
#include "core/paging.h"
#include "core/scheduler.h"
#include "device/ata.h"
#include "device/pci.h"
#include "lib/sync.h"

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
    
    bool ready = false;
    
    void initialize( uint32_t, uint32_t, uint32_t, uint32_t );
} ata_controller;

static struct __ata_channel {
    unsigned int channel_no;
    short        base;
    short        control;
    short        bus_master;
    bool         interrupt;
    uint8_t      selected_drive;
    uint32_t     prdt_phys;
    uint64_t     *prdt_virt;
    mutex        lock;
    process*     current_transfer_process;
    
    void initialize( short, short );
    void select( uint8_t );
    void wait_dma();
    void dma_write( void*, uint64_t, size_t);
    void dma_read( void*, uint64_t, size_t );
} ata_channels[2];

void __ata_channel::wait_dma() {
    while( io_inb(this->bus_master) & 1 ) { // loop until transfer is marked as complete
        process_switch_immediate();
    }
}

// send "select" command first!
void __ata_channel::dma_write( void* dma_buffer, uint64_t sector_start, size_t n_sectors ) {
    unsigned int n_bytes = n_sectors*512;
    this->lock.lock();
    this->wait_dma(); // wait on existing DMA transfers
    current_transfer_process = process_current;
    // place PRD in PRDT
    this->prdt_virt[0] = ((uint64_t)1<<63)+(((uint64_t)n_bytes)<<32)+((uint32_t)dma_buffer);
    io_outb(this->bus_master, 9); // DMA bit + Write bit
    // send DMA parameters to drive
    ata_write( this->channel_no, ATA_REG_SECCOUNT0, n_sectors&0xFF );
    ata_write( this->channel_no, ATA_REG_LBA3, (sector_start>>24)&0xFF );
    ata_write( this->channel_no, ATA_REG_LBA4, (sector_start>>32)&0xFF );
    ata_write( this->channel_no, ATA_REG_LBA5, (sector_start>>40)&0xFF );
    ata_write( this->channel_no, ATA_REG_SECCOUNT1, (n_sectors>>8)&0xFF );
    ata_write( this->channel_no, ATA_REG_LBA0, sector_start&0xFF );
    ata_write( this->channel_no, ATA_REG_LBA1, (sector_start>>8)&0xFF );
    ata_write( this->channel_no, ATA_REG_LBA2, (sector_start>>16)&0xFF );
    // send DMA command
    ata_write( this->channel_no, ATA_REG_COMMAND, ATA_CMD_WRITE_DMA_EXT );
    this->lock.unlock();
}

void __ata_channel::dma_read( void* dma_buffer, uint64_t sector_start, size_t n_sectors ) {
    unsigned int n_bytes = n_sectors*512;
    this->lock.lock();
    this->wait_dma(); // wait on existing DMA transfers
    // place PRD in PRDT
    this->prdt_virt[0] = ((uint64_t)1<<63)+(((uint64_t)n_bytes)<<32)+((uint32_t)dma_buffer);
    io_outb(this->bus_master, 1); // DMA bit
    // send DMA parameters to drive
    ata_write( this->channel_no, ATA_REG_SECCOUNT0, n_sectors&0xFF );
    ata_write( this->channel_no, ATA_REG_LBA3, (sector_start>>24)&0xFF );
    ata_write( this->channel_no, ATA_REG_LBA4, (sector_start>>32)&0xFF );
    ata_write( this->channel_no, ATA_REG_LBA5, (sector_start>>40)&0xFF );
    ata_write( this->channel_no, ATA_REG_SECCOUNT1, (n_sectors>>8)&0xFF );
    ata_write( this->channel_no, ATA_REG_LBA0, sector_start&0xFF );
    ata_write( this->channel_no, ATA_REG_LBA1, (sector_start>>8)&0xFF );
    ata_write( this->channel_no, ATA_REG_LBA2, (sector_start>>16)&0xFF );
    // send DMA command
    ata_write( this->channel_no, ATA_REG_COMMAND, ATA_CMD_READ_DMA_EXT );
    this->lock.unlock();
}

void __ata_channel::select( uint8_t select_val ) {
    if( this->selected_drive != select_val ) {
        this->selected_drive = select_val;
        
        // read status 4 times (giving it time to switch)
        for(int i=0;i<4;i++)
            io_inb( this->base + 7 );
    }
}

void __ata_channel::initialize( short base, short control ) {
    this->base = base;
    this->control = control;
}

void __ata_controller::initialize( uint32_t bar0, uint32_t bar1, uint32_t bar2, uint32_t bar3 ) {
    this->primary_channel   = bar0;
    this->primary_control   = bar1;
    this->secondary_channel = bar2;
    this->secondary_control = bar3;
    this->dma_control_base  = ATA_BUS_MASTER_START;
    
    uint8_t test_ch0 = ata_read(0, ATA_REG_STATUS);
    uint8_t test_ch1 = ata_read(1, ATA_REG_STATUS);
    
    ata_channels[0].channel_no = 0;
    ata_channels[1].channel_no = 1;
    
    pci_write_config_32( this->bus, this->device, this->func, 0x10, bar0 );
    pci_write_config_32( this->bus, this->device, this->func, 0x14, bar1 );
    pci_write_config_32( this->bus, this->device, this->func, 0x18, bar2 );
    pci_write_config_32( this->bus, this->device, this->func, 0x1C, bar3 );
    pci_write_config_32( this->bus, this->device, this->func, 0x20, ATA_BUS_MASTER_START );
    
    if( test_ch0 != 0xFF ) {
        // floating bus, there's nothing here
        ata_channels[0].initialize( bar0, bar1 );
        
        // set PRDT addresses
        io_outb( ATA_BUS_MASTER_START+4, ( ata_channels[0].prdt_phys & 0xFF ) );
        io_outb( ATA_BUS_MASTER_START+5, ( (ata_channels[0].prdt_phys>>8) & 0xFF ) );
        io_outb( ATA_BUS_MASTER_START+6, ( (ata_channels[0].prdt_phys>>16) & 0xFF ) );
        io_outb( ATA_BUS_MASTER_START+7, ( (ata_channels[0].prdt_phys>>24) & 0xFF ) );
        ata_channels[0].bus_master = ATA_BUS_MASTER_START;
        kprintf("ata: devices connected on channel 0 (not floating).\n");
    } else {
        kprintf("ata: no devices connected on channel 0.\n");
    }
    
    if( test_ch1 != 0xFF ) {
        ata_channels[1].initialize( bar2, bar3 );
        
        io_outb( ATA_BUS_MASTER_START+0xC, ( ata_channels[1].prdt_phys & 0xFF ) );
        io_outb( ATA_BUS_MASTER_START+0xD, ( (ata_channels[1].prdt_phys>>8) & 0xFF ) );
        io_outb( ATA_BUS_MASTER_START+0xE, ( (ata_channels[1].prdt_phys>>16) & 0xFF ) );
        io_outb( ATA_BUS_MASTER_START+0xF, ( (ata_channels[1].prdt_phys>>24) & 0xFF ) );
        ata_channels[1].bus_master = ATA_BUS_MASTER_START+8;
        kprintf("ata: devices connected on channel 1 (not floating).\n");
    } else {
        kprintf("ata: no devices connected on channel 1.\n");
    }
    
    if( (test_ch0 != 0xFF) || (test_ch1 != 0xFF) ) {
        page_frame *frame = pageframe_allocate(1);
        size_t prdt_virt = k_vmem_alloc(1);
        if( (frame != NULL) && (prdt_virt != NULL) ) {
            paging_set_pte( prdt_virt, frame->address, 0 );
            ata_channels[0].prdt_phys = frame->address;
            ata_channels[1].prdt_phys = frame->address+0x500;
            ata_channels[0].prdt_virt = ata_channels[1].prdt_virt = (uint64_t*)prdt_virt;
        }
        
        ata_controller.ready = true;
    }
}

void ata_write(uint8_t channel, uint8_t reg, uint8_t data) {
   if (reg > 0x07 && reg < 0x0C)
      ata_write(channel, ATA_REG_CONTROL, 0x80 | ata_channels[channel].interrupt ? 0 : 1);
   if (reg < 0x08)
      io_outb(ata_channels[channel].base  + reg - 0x00, data);
   else if (reg < 0x0C)
      io_outb(ata_channels[channel].base  + reg - 0x06, data);
   else if (reg < 0x0E)
      io_outb(ata_channels[channel].control  + reg - 0x0A, data);
   else if (reg < 0x16)
      io_outb(ata_channels[channel].bus_master + reg - 0x0E, data);
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
      result = io_inb(ata_channels[channel].control  + reg - 0x0A);
   else if (reg < 0x16)
      result = io_inb(ata_channels[channel].bus_master + reg - 0x0E);
   if (reg > 0x07 && reg < 0x0C)
      ata_write(channel, ATA_REG_CONTROL, ata_channels[channel].interrupt ? 0 : 1);
   return result;
}

bool ata_irq_handler() {
    for(int channel=0;channel<2;channel++) {
        uint8_t status = io_inb( ata_channels[channel].bus_master+2 );
        if( status & 4 ) { // did this channel cause the interrupt?
            if( (status & 1) == 0 ) { // is a DMA transfer complete?
                // send a signal to the transferring process
                message out( "ata_transfer_complete", (void*)(ata_channels[channel].prdt_virt[0]&0xFFFFFFFF), 0 );
                ata_channels[channel].current_transfer_process->send_message( out );
                io_outb(ata_channels[channel].bus_master, 0); // clear command register (mark DMA as complete)
            }
            return true;
        }
    }
    return false;
}

bool ata_begin_transfer( unsigned int channel, bool slave, bool write, void* dma_buffer, uint64_t sector_start, size_t n_sectors ) {
    if( channel > 1 ) {
        return false;
    }
    ata_channels[channel].lock.lock();
    ata_channels[channel].select( 0x40 | ((slave ? 1 : 0)<<4) );
    if( write )
        ata_channels[channel].dma_write( dma_buffer, sector_start, n_sectors );
    else
        ata_channels[channel].dma_read( dma_buffer, sector_start, n_sectors );
    ata_channels[channel].lock.unlock();
    return true;
}

void* ata_do_disk_read( unsigned int channel, bool slave, uint64_t sector_start, size_t n_sectors ) {
    unsigned int n_pages = n_sectors & 0xFFFFFFFC;
    if( (n_sectors & 3) > 0 ) { // round page count up
        n_pages++;
    }
    
    page_frame *frames = pageframe_allocate(n_pages);
    size_t      buffer_virt = k_vmem_alloc(n_pages);
    for(int i=0;i<n_pages;i++) {
        if( (i > 0) && (frames[i-1].address != (frames[i].address+0x1000)) ) {
            panic("ata: could not allocate contigous frames for DMA.\n");
        }
        paging_set_pte( buffer_virt+(i*0x1000), frames[i].address, 0 );
    }
    set_message_listen_status( "ata_transfer_complete", true );
    ata_begin_transfer( channel, slave, false, (void*)frames[0].address, sector_start, n_sectors );
    while(true) {
        message* msg = wait_for_message();
        if( strcmp(const_cast<char*>(msg->type), const_cast<char*>("ata_transfer_complete")) ) {
            uint32_t buffer_target = (uint32_t)(msg->data); // ata_transfer_complete messages have the buffer as their data element
            msg->data = NULL;
            delete msg;
            if( buffer_target == frames[0].address ) {
                return (void*)buffer_virt;
            }
        }
        delete msg;
    }
    return NULL;
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
            
            irq_add_handler( 14,(size_t)(&ata_irq_handler) );
            irq_add_handler( 15,(size_t)(&ata_irq_handler) );
            register_channel( "ata_transfer_complete", CHANNEL_MODE_BROADCAST );
            return;
        }
    }
}