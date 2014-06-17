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
    unsigned char bus    = 0;
    unsigned char device = 0;
    unsigned char func   = 0;
    unsigned char progif = 0;
    
    unsigned short primary_channel   = 0;
    unsigned short primary_control   = 0;
    unsigned short secondary_channel = 0;
    unsigned short secondary_control = 0;
    unsigned short dma_control_base  = 0;
    
    bool ready = false;
    
    void initialize( uint32_t, uint32_t, uint32_t, uint32_t );
} ata_controller;

static struct __ata_channel {
    unsigned int          channel_no;
    short                 base;
    short                 control;
    short                 bus_master;
    bool                  interrupt;
    uint8_t               selected_drive = 0;
    uint32_t              prdt_phys;
    uint64_t              *prdt_virt;
	uint32_t              prdt_n_entries;
	uint32_t              prdt_current;
    mutex                 lock;
    dma_request*          current_transfer = NULL;
    vector<dma_request*> *read_queue = NULL;
    vector<dma_request*> *write_queue = NULL;
    vector<dma_request*> *current_operation = NULL; // true - reading, false - writing
    vector<dma_request*> *current_prdt_queue = NULL;
    bool                  currently_idle = true;
    process*              delayed_starter;
    
    void                 *dev1_ident;
    void                 *dev2_ident;
    bool                  dev1_lba48;
    bool                  dev2_lba48;
    char                  dev1_model[41];
    char                  dev2_model[41];
    char                  dev1_serial[21];
    char                  dev2_serial[21];
    
    void initialize( short, short, short );
    void do_identify();
    void select( uint8_t );
    void cache_flush();
    void wait_dma();
    void wait_bsy();
    
    void req_enqueue( dma_request* );
    void fill_prdt();
    void swap_operations();
    void dma_start( bool );
} ata_channels[2];

void __ata_channel::req_enqueue( dma_request* req ) {
    this->lock.lock();
    if( req->read ) {
        this->read_queue->add(req);
    } else {
        this->write_queue->add(req);
    }
    if( this->currently_idle ) {
        if( req->read ) {
            this->current_operation = this->read_queue;
        } else {
            this->current_operation = this->write_queue;
        }
        this->currently_idle = false;
        this->dma_start( true );
    }
    this->lock.unlock();
}

void __ata_channel::fill_prdt() {
    this->lock.lock();
    this->prdt_n_entries = this->current_operation->count();
    this->prdt_current = 0;
    io_outb(this->bus_master, 0); // terminate DMA mode (so we can change the PRDT safely)
    if( this->prdt_n_entries < 0xA0 ) {
        for(unsigned int i=0;i<this->current_operation->count();i++) {
            dma_request *current = this->current_operation->remove();
            this->prdt_virt[i] = (((uint64_t)((uint16_t)(current->n_sectors*512)))<<32) | (uint64_t)current->buffer_phys; // cast to both avoid and uilize integer overflow (use for the multiply, avoid for the shift)
            this->current_prdt_queue->add_end( current );
        }
        this->prdt_virt[this->prdt_n_entries-1] |= ((uint64_t)1<<63);
    } else {
        this->prdt_n_entries = 0xA0;
        for(unsigned int i=0;i<0xA0;i++) {
            dma_request *current = this->current_operation->remove();
            this->prdt_virt[i] = (((uint64_t)((uint16_t)(current->n_sectors*512)))<<32) | (uint64_t)current->buffer_phys;
            this->current_prdt_queue->add_end( current );
        }
        this->prdt_virt[0x98] |= ((uint64_t)1<<63);
    }
    if( this->current_operation == this->read_queue )
        io_outb(this->bus_master, 9); // set DMA + read bits
    else
        io_outb(this->bus_master, 1); // set DMA bit
    this->lock.unlock();
}

void __ata_channel::swap_operations() {
    this->lock.lock();
    if( this->current_operation == this->read_queue ) {
        this->current_operation = this->write_queue;
    } else {
        this->current_operation = this->read_queue;
    }
    if( this->current_operation->count() <= 0 ) { // if there aren't any transfers on this queue, then check the other one
        if( this->current_operation == this->read_queue ) {
            this->current_operation =  this->write_queue;
        } else {
            this->current_operation =  this->read_queue;
        }
        if( this->current_operation->count() <= 0 ) { // if there aren't any on either queue, then mark ourselves as idle
            this->currently_idle = true;
            this->current_prdt_queue->clear();
            this->prdt_n_entries = 0;
            this->prdt_current = 0;
        } else {
            this->fill_prdt();
        }
    } else {
        this->fill_prdt();
    }
    this->lock.unlock();
}

void __ata_channel::do_identify() {
    this->dev1_ident = kmalloc(512);
    this->dev2_ident = kmalloc(512);
    if( this->dev1_ident != NULL ) {
        this->select( 0xA0 );
        io_outb( this->base+2, 0 );
        io_outb( this->base+3, 0 );
        io_outb( this->base+4, 0 );
        io_outb( this->base+5, 0 );
        io_outb( this->base+7, ATA_CMD_IDENTIFY );
        
        uint8_t stat = io_inb( this->control );
        if( stat > 0 ) {
            // okay, there actually IS something here
            while( (io_inb( this->control ) & 0x80) > 0 ); // wait for BSY to clear
            if( !((io_inb( this->base+4 ) > 0) || (io_inb( this->base+5 ) > 0)) ) { // check for non-spec ATAPI devices
                while( (io_inb( this->control ) & 0x09) == 0 ); // wait for either DRQ or ERR to set
                if( (io_inb( this->control ) & 0x01) == 0 ) { // is ERR clear?
                    ata_do_pio_sector_transfer( this->channel_no, this->dev1_ident, false ); // read 256 words of PIO data
                }
            }
        }
    }
    
    if( this->dev2_ident != NULL ) {
        this->select( 0xB0 );
        io_outb( this->base+2, 0 );
        io_outb( this->base+3, 0 );
        io_outb( this->base+4, 0 );
        io_outb( this->base+5, 0 );
        io_outb( this->base+7, ATA_CMD_IDENTIFY );
        
        uint8_t stat = io_inb( this->control );
        if( stat > 0 ) {
            while( (io_inb( this->control ) & 0x80) > 0 );
            if( !((io_inb( this->base+4 ) > 0) || (io_inb( this->base+5 ) > 0)) ) {
                while( (io_inb( this->control ) & 0x09) == 0 );
                if( (io_inb( this->control ) & 0x01) == 0 ) {
                    ata_do_pio_sector_transfer( this->channel_no, this->dev2_ident, false );
                }
            }
        }
    }
}

void __ata_channel::dma_start( bool no_swap ) {
    bool is_lba48 = false;
    kprintf("ata: beginning DMA access.\n");
    this->lock.lock();
    if( !no_swap ) {
        this->prdt_current++;
        if( this->prdt_current >= this->prdt_n_entries ) { // no transfers left for the current operation, do other transfers now
            this->swap_operations();
            if( this->currently_idle ) { // no transfers at all left
                return;
            }
        }
    } else {
        this->fill_prdt();
    }
    if( this->current_transfer != NULL ) {
        this->cache_flush();
        this->current_transfer->status = true;
        message out("ata_transfer_complete", this->current_transfer->buffer_phys, 0);
        this->current_transfer->requesting_process->send_message( out );
    }
    this->current_transfer = current_prdt_queue->remove();
    if( this->current_transfer == NULL ) {
        return this->dma_start( false ); // swap operations
    }
    kprintf("ata: selecting drive.\n");
    
    if( this->current_transfer->to_slave ) {
        is_lba48 = this->dev2_lba48;
        if( !this->dev2_lba48 && ( this->current_transfer->sector_start > 0x0FFFFFFF) ) { // are we trying to do LBA48 transfers on an LBA28 drive?
            kprintf("ata: transfer to slave drive attempted to access past LBA28 limit!");
            return this->dma_start( true );
        }
    } else {
        is_lba48 = this->dev1_lba48;
        if( !this->dev1_lba48 && ( this->current_transfer->sector_start > 0x0FFFFFFF) ) { // are we trying to do LBA48 transfers on an LBA28 drive?
            kprintf("ata: transfer to master drive attempted to access past LBA28 limit!");
            return this->dma_start( true );
        }
    }
    this->wait_bsy();
    // send DMA parameters to drive
    if( is_lba48 ) {
        this->select( 0x40 + ( this->current_transfer->to_slave ? 0x10 : 0 ) );
        kprintf("ata: sending DMA parameters.\n * n_sectors = %u\n * sector_start(LBA48) = %u\n", this->current_transfer->n_sectors, this->current_transfer->sector_start);
        io_outb( this->base+2, (this->current_transfer->n_sectors>>8)    & 0xFF );
        io_outb( this->base+3, (this->current_transfer->sector_start>>24)& 0xFF );
        io_outb( this->base+4, (this->current_transfer->sector_start>>32)& 0xFF );
        io_outb( this->base+5, (this->current_transfer->sector_start>>40)& 0xFF );
        
        io_outb( this->base+2, this->current_transfer->n_sectors         & 0xFF );
        io_outb( this->base+3, this->current_transfer->sector_start      & 0xFF );
        io_outb( this->base+4, (this->current_transfer->sector_start>>8) & 0xFF );
        io_outb( this->base+5, (this->current_transfer->sector_start>>16)& 0xFF );
        // send DMA command
        kprintf("ata: sending DMA command.\n");
        if( this->current_transfer->read )
            io_outb( this->base+7, ATA_CMD_READ_DMA_EXT );
        else
            io_outb( this->base+7, ATA_CMD_WRITE_DMA_EXT );
    } else {
        this->select( 0xE0  | ( this->current_transfer->to_slave ? 0x10 : 0 ) | ( (this->current_transfer->sector_start >> 24) & 0x0F ) );
        kprintf("ata: sending DMA parameters.\n * n_sectors = %u\n * sector_start(LBA28) = %u\n", this->current_transfer->n_sectors, this->current_transfer->sector_start);
        io_outb( this->base+2, this->current_transfer->n_sectors         & 0xFF );
        io_outb( this->base+3, this->current_transfer->sector_start      & 0xFF );
        io_outb( this->base+4, (this->current_transfer->sector_start>>8) & 0xFF );
        io_outb( this->base+5, (this->current_transfer->sector_start>>16)& 0xFF );
        // send DMA command
        kprintf("ata: sending DMA command.\n");
        if( this->current_transfer->read )
            io_outb( this->base+7, ATA_CMD_READ_DMA );
        else
            io_outb( this->base+7, ATA_CMD_WRITE_DMA );
    }
    this->lock.unlock();
}

void irq14_delayed_starter() {
    while(true) {
        process_current->state = process_state::waiting;
        process_switch_immediate();
        ata_channels[0].dma_start(false);
        kprintf("IRQ14!\n");
    }
}

void irq15_delayed_starter() {
    while(true) {
        process_current->state = process_state::waiting;
        process_switch_immediate();
        ata_channels[1].dma_start(false);
        kprintf("IRQ15!\n");
    }
}

void ata_error_checker() {
    while(true) {
        uint8_t stat_0 = io_inb( ata_channels[0].base+1 );
        uint8_t stat_1 = io_inb( ata_channels[1].base+1 );
        if( ((stat_0 & 0x80) == 0) && ((stat_0 && 1) > 0) ) {
            kprintf("ata: Error on channel 0.\n");
        }
        if( ((stat_0 & 0x80) == 0) && ((stat_0 && 1) > 0) ) {
            kprintf("ata: Error on channel 1.\n");
        }
        process_switch_immediate();
    }
}

bool dma_irq( unsigned int channel_no ) {
    uint8_t status = io_inb( ata_channels[channel_no].bus_master+2 );
    if( status & 4 ) { // did this channel cause the interrupt?
        io_inb( ata_channels[channel_no].base+7 ); // if so, then read regular status register
        ata_channels[channel_no].delayed_starter->state = process_state::runnable;
        process_add_to_runqueue(ata_channels[channel_no].delayed_starter);
        return true;
    }
    return false;
}

bool irq14_handler() { /* kprintf("IRQ14!\n"); */ return dma_irq(0); }
bool irq15_handler() { /* kprintf("IRQ15!\n"); */ return dma_irq(1); }
bool ata_dual_handler() { kprintf("IRQ14!\n"); return ( dma_irq(0) || dma_irq(1) ); }

void __ata_channel::wait_dma() {
    while( io_inb(this->bus_master) & 1 ) { // loop until transfer is marked as complete
        process_switch_immediate();
    }
}

void __ata_channel::wait_bsy() {
    uint64_t ts = get_sys_time_counter();
    while( (io_inb( this->control )&0x80) > 0 ) {
        if( get_sys_time_counter() > (ts+5000) ) {// five second timeout
            io_outb( this->control, 0x04 ); // set Software Reset
            for(int i=0;i<4;i++) // 400ns delay
                io_inb( this->control );
            io_outb( this->control, 0 ); // clear Software Reset
            break;
        }
        process_switch_immediate(); // wait for BSY to clear
    }
}

void __ata_channel::select( uint8_t select_val ) {
    if( this->selected_drive != select_val ) {
        this->selected_drive = select_val;
        io_outb( this->base+6, select_val );
        // read status 4 times (giving it time to switch)
        for(int i=0;i<4;i++)
            io_inb( this->control );
    }
}

void __ata_channel::cache_flush() {
    this->select( ATA_MASTER );
    this->wait_bsy();
    if( this->dev1_lba48 ) {
        io_outb( this->base+7, ATA_CMD_CACHE_FLUSH_EXT );
    } else {
        io_outb( this->base+7, ATA_CMD_CACHE_FLUSH );
    }
    
    this->select( ATA_SLAVE );
    this->wait_bsy();
    if( this->dev2_lba48 ) {
        io_outb( this->base+7, ATA_CMD_CACHE_FLUSH_EXT );
    } else {
        io_outb( this->base+7, ATA_CMD_CACHE_FLUSH );
    }
}

void __ata_channel::initialize( short base, short control, short bus_master ) {
    this->base = base;
    this->control = control;
    
    this->read_queue         = new vector<dma_request*>;
    this->write_queue        = new vector<dma_request*>;
    this->current_prdt_queue = new vector<dma_request*>;
    this->bus_master         = bus_master;
    
    // set PRDT addresses
    io_outb( this->bus_master+4,  this->prdt_phys      & 0xFF );
    io_outb( this->bus_master+5, (this->prdt_phys>>8)  & 0xFF );
    io_outb( this->bus_master+6, (this->prdt_phys>>16) & 0xFF );
    io_outb( this->bus_master+7, (this->prdt_phys>>24) & 0xFF );
    
    this->do_identify();
    this->dev1_lba48 = ( (((uint16_t*)this->dev1_ident)[83] & 0x400) );
    this->dev2_lba48 = ( (((uint16_t*)this->dev2_ident)[83] & 0x400) );
    
    for(unsigned int i=0;i<40;i+=2) {
        this->dev1_model[i+1] = ((char*)this->dev1_ident)[54+i];
        this->dev1_model[i] = ((char*)this->dev1_ident)[55+i];
        
        this->dev2_model[i+1] = ((char*)this->dev2_ident)[54+i];
        this->dev2_model[i] = ((char*)this->dev2_ident)[55+i];
    }
    
    this->dev1_model[40] = 0;
    this->dev2_model[40] = 0;
    
    for(unsigned int i=0;i<20;i+=2) {
        this->dev1_serial[i+1] = ((char*)this->dev1_ident)[20+i];
        this->dev1_serial[i] = ((char*)this->dev1_ident)[21+i];
        
        this->dev2_serial[i+1] = ((char*)this->dev2_ident)[20+i];
        this->dev2_serial[i] = ((char*)this->dev2_ident)[21+i];
    }
    
    this->dev1_serial[20] = 0;
    this->dev2_serial[20] = 0;
    
    uint64_t dev1_n_sectors;
    uint64_t dev2_n_sectors;
    kprintf("ata: channel %u devices: \n", this->channel_no);
    if( this->dev1_lba48 ) {
        dev1_n_sectors = ((uint64_t*)this->dev1_ident)[25];
    } else {
        dev1_n_sectors = ((uint32_t*)this->dev1_ident)[30];
    }
    if( this->dev2_lba48 ) {
        dev2_n_sectors = ((uint64_t*)this->dev2_ident)[25];
    } else {
        dev2_n_sectors = ((uint32_t*)this->dev2_ident)[30];
    }
    kprintf("    * master: %s (SN: %s), %s, %u sectors addressable\n", this->dev1_model, this->dev1_serial, ( this->dev1_lba48 ? "LBA48 supported" : "LBA48 not supported" ), dev1_n_sectors);
    kprintf("    * slave:  %s (SN: %s), %s, %u sectors addressable\n", this->dev2_model, this->dev2_serial, ( this->dev2_lba48 ? "LBA48 supported" : "LBA48 not supported" ), dev2_n_sectors);
}

void __ata_controller::initialize( uint32_t bar0, uint32_t bar1, uint32_t bar2, uint32_t bar3 ) {
    this->primary_channel   = bar0;
    this->primary_control   = bar1;
    this->secondary_channel = bar2;
    this->secondary_control = bar3;
    this->dma_control_base  = ATA_BUS_MASTER_START;
    
    // PCI programming interface register:
    //pci_write_config_32( this->bus, this->device, this->func, 0x09, 0x5 ); // enable Native PCI mode
    // BARs:
    pci_write_config_32( this->bus, this->device, this->func, 0x10, bar0 );
    pci_write_config_32( this->bus, this->device, this->func, 0x14, bar1 );
    pci_write_config_32( this->bus, this->device, this->func, 0x18, bar2 );
    pci_write_config_32( this->bus, this->device, this->func, 0x1C, bar3 );
    pci_write_config_32( this->bus, this->device, this->func, 0x20, ATA_BUS_MASTER_START );
    // PCI command register:
    pci_write_config_32( this->bus, this->device, this->func, 0x04, 0x5 ); // Bus Master Enable | I/O Space Enable
    
    // determine IRQs:
    pci_read_config_8( this->bus, this->device, this->func, 0x3C );
    pci_write_config_8( this->bus, this->device, this->func, 0x3C, 0xFE );
    if( pci_read_config_8( this->bus, this->device, this->func, 0x3C ) == 0xFE ) {
        // device needs IRQ assignment (we'll just use IRQ14 in this case)
        kprintf("ata: controller IRQ assignment required\n");
        irq_add_handler(14, (size_t)(&ata_dual_handler));
        pci_write_config_8( this->bus, this->device, this->func, 0x3C, 14 );
    } else {
        // device doesn't need an IRQ assignment, is it PATA?
        if( (this->progif == 0x8A) || (this->progif == 0x80) ) {
            // it's PATA, so we just assign IRQs 14 and 15.
            kprintf("ata: controller is PATA, no IRQ assignment required\n");
            irq_add_handler(14, (size_t)(&irq14_handler));
            irq_add_handler(15, (size_t)(&irq15_handler));
        } else {
            // in this case we must be using the APIC (Interrupt Line field is R/O)
            // well, just do nothing in this case (we don't support the APIC yet)
            kprintf("ata: Interrupt Line field is read-only\n");
        }
    }
    
    uint8_t test_ch0 = io_inb(bar0+7);
    uint8_t test_ch1 = io_inb(bar2+7);
    
    ata_channels[0].channel_no = 0;
    ata_channels[1].channel_no = 1;
    
    if( (test_ch0 != 0xFF) || (test_ch1 != 0xFF) ) {
        page_frame *frame = pageframe_allocate(1);
        size_t prdt_virt = k_vmem_alloc(1);
        if( (frame != NULL) && (prdt_virt != NULL) ) {
            paging_set_pte( prdt_virt, frame->address, 0 );
            ata_channels[0].prdt_phys = frame->address;
            ata_channels[0].prdt_virt = (uint64_t*)prdt_virt;
            ata_channels[1].prdt_phys = frame->address+0x500;
            ata_channels[1].prdt_virt = (uint64_t*)(prdt_virt+0x500);
            kprintf("PRDTs located at:\n");
            kprintf(" * virtual 0x%p and 0x%p.\n", ata_channels[0].prdt_virt, ata_channels[1].prdt_virt);
            kprintf(" * physical 0x%x and 0x%x.\n", ata_channels[0].prdt_phys, ata_channels[1].prdt_phys);
        }
    }
    
    if( test_ch0 != 0xFF ) {
        ata_channels[0].initialize( bar0, bar1, ATA_BUS_MASTER_START );
        ata_channels[0].delayed_starter = new process( (size_t)&irq14_delayed_starter, 0, false, "ata_ch0_delayedstarter", NULL, 0 );

        // reset control register
        io_outb( bar1, 0 );
        kprintf("ata: devices connected on channel 0 (not floating).\n");
    } else {
        // floating bus, there's nothing here
        kprintf("ata: no devices connected on channel 0.\n");
    }
    
    if( test_ch1 != 0xFF ) {
        ata_channels[1].initialize( bar2, bar3, ATA_BUS_MASTER_START+8 );
        ata_channels[1].delayed_starter = new process( (size_t)&irq15_delayed_starter, 0, false, "ata_ch1_delayedstarter", NULL, 0 );
        
        io_outb( bar3, 0 );
        kprintf("ata: devices connected on channel 1 (not floating).\n");
    } else {
        kprintf("ata: no devices connected on channel 1.\n");
    }
    
    ata_controller.ready = true;
}
/*
void ata_write(uint8_t channel, uint8_t reg, uint8_t data) {
   if (reg < 0x08)
      io_outb(ata_channels[channel].base  + reg - 0x00, data);
   else if (reg < 0x0C)
      io_outb(ata_channels[channel].base  + reg - 0x06, data);
   else if (reg < 0x0E)
      io_outb(ata_channels[channel].control  + reg - 0x0A, data);
   else if (reg < 0x16)
      io_outb(ata_channels[channel].bus_master + reg - 0x0E, data);
}

uint8_t ata_read(uint8_t channel, uint8_t reg) {
   uint8_t result;
   if (reg < 0x08)
      result = io_inb(ata_channels[channel].base + reg - 0x00);
   else if (reg < 0x0C)
      result = io_inb(ata_channels[channel].base  + reg - 0x06);
   else if (reg < 0x0E)
      result = io_inb(ata_channels[channel].control  + reg - 0x0A);
   else if (reg < 0x16)
      result = io_inb(ata_channels[channel].bus_master + reg - 0x0E);
   return result;
}
*/

dma_buffer::dma_buffer( size_t sz ) {
    this->size = sz;
    this->n_frames = ( (sz-(sz%0x1000)) / 0x1000 )+1;
    
    this->frames = pageframe_allocate( this->n_frames );
    this->buffer_virt = (void*)k_vmem_alloc( n_frames );
    this->buffer_phys = (void*)this->frames[0].address;
    for(unsigned int i=0;i<this->n_frames;i++) {
        if( !((i <= 0) || (this->frames[i].address == (this->frames[i-1].address+0x1000))) )
            panic("ata: Could not allocate contiguous frames for DMA buffer!\n");
        paging_set_pte( ((size_t)buffer_virt)+(i*0x1000), this->frames[i].address,0 );
    }
};

/*
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
*/
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
            
            register_channel( "ata_transfer_complete", CHANNEL_MODE_BROADCAST );
            return;
        }
    }
}

void ata_start_request( dma_request* req, unsigned int channel_no ) {
    if( (channel_no != 0) && (channel_no != 1) )
        return;
    ata_channels[channel_no].req_enqueue( req );
}

void ata_do_pio_sector_transfer( int channel, void* buffer, bool write ) {
    uint16_t* current = (uint16_t*)buffer;
    for(unsigned int i=0;i<256;i++) {
        if( write )
            io_outw( ata_channels[channel].base, *current++ );
        else
            *current++ = io_inw( ata_channels[channel].base );
    }
}

void ata_do_cache_flush() {
    ata_channels[0].cache_flush();
    ata_channels[1].cache_flush();
}