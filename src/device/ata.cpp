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
    unsigned int          channel_no;
    short                 base;
    short                 control;
    short                 bus_master;
    bool                  interrupt;
    uint8_t               selected_drive;
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
    
    void initialize( short, short );
    void select( uint8_t );
    void wait_dma();
    
    void req_enqueue( dma_request*, bool, unsigned int );
    void fill_prdt( vector<dma_request*> * );
    void swap_operations();
    void dma_start( bool );
} ata_channels[2];

typedef struct dma_request {
    void        *buffer_phys;
    uint64_t     sector_start;
    size_t       n_sectors;
    bool         to_slave;
    bool         status;
    process*     requesting_process;
    
    dma_request( void* buf, uint64_t secst, size_t nsec, bool sl ) : buffer_phys(buf), sector_start(secst), n_sectors(nsec), to_slave(st), requesting_process(process_current) {};
} dma_request;

void __ata_channel::req_enqueue( dma_request* req, bool read, unsigned int channel_no ) {
    this->lock.lock();
    if( read ) {
        this->read_queue.add(req);
    } else {
        this->write_queue.add(req);
    }
    if( this->currently_idle ) {
        if( read ) {
            this->current_operation = this->read_queue;
        } else {
            this->current_operation = this->write_queue;
        }
        this->fill_prdt( this->current_operation );
        this->currently_idle = false;
        if( !read )
            io_outb(this->bus_master, 1); // set DMA bit
        else
            io_outb(this->bus_master, 9); // set DMA + write bits
        this->dma_start( true );
    }
    this->lock.unlock();
}

void __ata_channel::fill_prdt( vector<dma_request*>* queue ) {
    this->lock.lock();
    this->prdt_n_entries = queue->count()-1;
    this->prdt_current = 0;
    if( this->prdt_n_entries < 0xA0 ) {
        for(unsigned int i=0;i<queue.count();i++) {
            dma_request *current = queue->remove();
            this->prdt_virt[i] = (((uint64_t)(current->n_sectors*512))<<32)+((uint32_t)current->buffer_phys);
            if( queue.count() == 0)
                this->prdt_virt[i] |= (uint64_t)(1<<63);
            this->current_prdt_queue->add_end( current );
        }
    } else {
        this->prdt_n_entries = 0xA0;
        for(unsigned int i=0;i<0xA0;i++) {
            dma_request *current = queue->remove();
            this->prdt_virt[i] = (((uint64_t)(current->n_sectors*512))<<32)+((uint32_t)current->buffer_phys);
            this->current_prdt_queue->add_end( current );
        }
        this->prdt_virt[0x98] |= (uint64_t)(1<<63);
    }
    this->lock.unlock();
}

void __ata_channel::swap_operations() 
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
        } else
            this->fill_prdt( current_operation );
    } else
        this->fill_prdt( current_operation );
    this->lock.unlock();
}

void __ata_channel::dma_start( bool no_swap ) {
    this->lock.lock();
    if( !no_swap ) {
        this->prdt_n_entries++;
        if( this->prdt_n_entries >= this->prdt_current ) { // no transfers left for the current operation, do other transfers now
            this->swap_operations();
            if( this->currently_idle ) { // no transfers at all left
                io_outb(this->bus_master, 0); // terminate DMA mode
                return;
            }
        }
    }
    if( this->current_transfer != NULL ) {
        this->current_transfer->state = true;
        message out("ata_transfer_complete", buffer_phys, 0);
        this->current_transfer->requesting_process->send_message( out );
    }
    this->current_transfer = current_prdt_queue.remove();
    if( this->current_transfer == NULL ) {
        return this->dma_start(no_swap); // swap operations again
    }
    this->select( 0x40 | ((this->current_transfer->to_slave ? 1 : 0)<<4) );
    // send DMA parameters to drive
    ata_write( this->channel_no, ATA_REG_SECCOUNT0, this->current_transfer->n_sectors&0xFF );
    ata_write( this->channel_no, ATA_REG_LBA3, (this->current_transfer->sector_start>>24)&0xFF );
    ata_write( this->channel_no, ATA_REG_LBA4, (this->current_transfer->sector_start>>32)&0xFF );
    ata_write( this->channel_no, ATA_REG_LBA5, (this->current_transfer->sector_start>>40)&0xFF );
    ata_write( this->channel_no, ATA_REG_SECCOUNT1, (this->current_transfer->n_sectors>>8)&0xFF );
    ata_write( this->channel_no, ATA_REG_LBA0, this->current_transfer->sector_start&0xFF );
    ata_write( this->channel_no, ATA_REG_LBA1, (this->current_transfer->sector_start>>8)&0xFF );
    ata_write( this->channel_no, ATA_REG_LBA2, (this->current_transfer->sector_start>>16)&0xFF );
    // send DMA command
    ata_write( this->channel_no, ATA_REG_COMMAND, ATA_CMD_WRITE_DMA_EXT );
    this->lock.unlock();
}

void irq14_delayed_starter() {
    while(true) {
        ata_channels[0].dma_start(false);
        process_current->state = process_state::waiting;
        process_switch_immediate();
    }
}

void irq15_delayed_starter() {
    while(true) {
        ata_channels[1].dma_start(false);
        process_current->state = process_state::waiting;
        process_switch_immediate();
    }
}

bool dma_irq( unsigned int channel_no ) {
    uint8_t status = io_inb( ata_channels[channel_no].bus_master+2 );
    if( status & 4 ) { // did this channel cause the interrupt?
        ata_channels[channel_no].delayed_starter->state = process_state::waiting;
        process_add_to_runqueue(ata_channels[channel_no].delayed_starter);
        return true;
    }
    return false;
}

bool irq14_handler() { return dma_irq(0); }
bool irq15_handler() { return dma_irq(1); }

void __ata_channel::wait_dma() {
    while( io_inb(this->bus_master) & 1 ) { // loop until transfer is marked as complete
        process_switch_immediate();
    }
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
    
    this->read_queue        = new vector<dma_request*>;
    this->write_queue       = new vector<dma_request*>;
    this->current_prdt_queue        = new vector<dma_request*>;
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
        ata_channels[0].delayed_starter = new process( (size_t)&irq14_delayed_starter, 0, false, "ata_ch0_delayedstarter", NULL, 0 );
        
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
        ata_channels[1].delayed_starter = new process( (size_t)&irq15_delayed_starter, 0, false, "ata_ch1_delayedstarter", NULL, 0 );
        
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
            
            irq_add_handler( 14,(size_t)(&irq14_handler) );
            irq_add_handler( 15,(size_t)(&irq15_handler) );
            register_channel( "ata_transfer_complete", CHANNEL_MODE_BROADCAST );
            return;
        }
    }
}