// ata.cpp -- ATA Storage driver

#include "includes.h"
#include "arch/x86/sys.h"
#include "arch/x86/irq.h"
#include "core/io.h"
#include "core/paging.h"
#include "core/scheduler.h"
#include "device/ata.h"
#include "device/pci.h"
#include "device/pit.h"
#include "device/vga.h"
#include "lib/sync.h"

static process* err_chk;
static process* dma_chk;

void ata_error_checker();
void ata_dma_poller();

// I assume that there's only one ATA controller in a system.
struct __ata_controller {
    pci_device *device;
    
    unsigned short primary_channel   = 0;
    unsigned short primary_control   = 0;
    unsigned short secondary_channel = 0;
    unsigned short secondary_control = 0;
    unsigned short dma_control_base  = 0;
    
    bool ready = false;
    
    void initialize();
} ata_controller;

struct ata_device {
    void* ident;
    uint64_t n_sectors;
    bool  lba48;
    bool  present;
    char  model[41];
    char  serial[21];
    
    void initialize();
};

struct __ata_channel {
    unsigned int          channel_no;
    short                 base;
    short                 control;
    short                 bus_master;
    bool                  interrupt;
    bool                  currently_idle = true;
    uint8_t               selected_drive = 0;
    uint32_t              prdt_phys;
    uint32_t              *prdt_virt;
	uint32_t              prdt_n_entries;
	uint32_t              prdt_current;
    mutex                 lock;
    ata_transfer_request* current_transfer = NULL;
    vector<ata_transfer_request*> read_queue;
    vector<ata_transfer_request*> write_queue;
    bool                  current_operation = false; // false - read, true - write
    process*              delayed_starter;
    
    bool                  devices_present = false;
    ata_device            devices[2];
    bool                  error_handled = false;
    
    void initialize( short, short, short );
    void do_identify();
    void select( uint8_t );
    void cache_flush();
    void wait_dma();
    void wait_bsy();
    void enqueue_request( ata_transfer_request* );
    
    void transfer_start( ata_transfer_request* );
    void transfer_cycle();
    bool transfer_available();
} ata_channels[2];

struct ata_io_disk : public io_disk {
    __ata_channel *channel;
    bool           to_slave;
    
    void send_request( transfer_request* );
    unsigned int  get_sector_size() { return 512; };
    unsigned int  get_total_size();
};

unsigned int ata_io_disk::get_total_size() {
    if( !this->to_slave ) {
        return (this->channel->devices[0].n_sectors)*512;
    } else {
        return (this->channel->devices[1].n_sectors)*512;
    }
}

void ata_io_disk::send_request( transfer_request* req ) {
    ata_transfer_request *new_req = new ata_transfer_request( *req );
    new_req->to_slave = this->to_slave;
    this->channel->enqueue_request( new_req );
}

void __ata_channel::do_identify() {
    this->devices[0].ident = kmalloc(512);
    this->devices[1].ident = kmalloc(512);
    
    kassert( ((this->devices[0].ident != NULL) && (this->devices[1].ident != NULL)), "Could not allocate space for IDENTIFY command!\n" );
    
    this->error_handled = false;
    
    this->select( 0xA0 );
    io_outb( this->base+2, 0 );
    io_outb( this->base+3, 0 );
    io_outb( this->base+4, 0 );
    io_outb( this->base+5, 0 );
    io_outb( this->base+7, ATA_CMD_IDENTIFY );
    
    this->devices[0].present = false;
    
    uint8_t stat = io_inb( this->control );
    if( stat > 0 ) {
        // okay, there actually IS something here
        if( !( (io_inb(this->base + 7) & 1) || ( (io_inb(this->base+4) == 0x14) && (io_inb(this->base+5) == 0xEB) ) ) ) { // is this an ATAPI / SATA device?
            // if not, continue..
            while( (io_inb( this->control ) & 0x80) > 0 ); // wait for BSY to clear
            if( !((io_inb( this->base+4 ) > 0) || (io_inb( this->base+5 ) > 0)) ) { // check for non-spec ATAPI devices
                while( (io_inb( this->control ) & 0x09) == 0 ); // wait for either DRQ or ERR to set
                if( (io_inb( this->control ) & 0x01) == 0 ) { // is ERR clear?
                    ata_do_pio_sector_transfer( this->channel_no, this->devices[0].ident, false ); // read 256 words of PIO data
                    this->devices[0].present = true;
                }
            }
        } else {
            this->error_handled = true;
        }
    }
    
    this->select( 0xB0 );
    io_outb( this->base+2, 0 );
    io_outb( this->base+3, 0 );
    io_outb( this->base+4, 0 );
    io_outb( this->base+5, 0 );
    io_outb( this->base+7, ATA_CMD_IDENTIFY );
    
    this->devices[1].present = false;
    
    stat = io_inb( this->control );
    if( stat > 0 ) {
        if( !( (io_inb(this->base + 7) & 1) || ( (io_inb(this->base+4) == 0x14) && (io_inb(this->base+5) == 0xEB) ) ) ) {
            while( (io_inb( this->control ) & 0x80) > 0 );
            if( !((io_inb( this->base+4 ) > 0) || (io_inb( this->base+5 ) > 0)) ) {
                while( (io_inb( this->control ) & 0x09) == 0 );
                if( (io_inb( this->control ) & 0x01) == 0 ) {
                    ata_do_pio_sector_transfer( this->channel_no, this->devices[1].ident, false );
                    this->devices[1].present = true;
                }
            }
        } else {
            this->error_handled = true;
        }
    }
}

void __ata_channel::enqueue_request( ata_transfer_request* req ) {
    if( req->read ) {
        this->read_queue.add_end( req );
    } else {
        this->write_queue.add_end( req );
    }
    
    if( this->currently_idle ) {
        this->current_operation = req->read;
        this->currently_idle = false;
        //this->transfer_cycle();
    }
    this->delayed_starter->state = process_state::runnable; // indirectly schedule ourselves to run later
    process_add_to_runqueue(this->delayed_starter);
}

void __ata_channel::transfer_cycle() {
    if( ata_controller.ready ) {
        this->current_operation = !this->current_operation;
        if( (this->current_operation ? this->write_queue : this->read_queue).count() == 0 ) {
            this->current_operation = !this->current_operation;
            if( (this->current_operation ? this->write_queue : this->read_queue).count() == 0 ) {
                this->current_transfer = NULL;
                this->currently_idle = true;
                return;
            }
        }
        
        if( this->current_transfer != NULL ) {
            this->current_transfer->status = true;
            if( this->current_transfer->requesting_process != NULL ) {
                message out("transfer_complete", this->current_transfer, sizeof(ata_transfer_request));
                this->current_transfer->requesting_process->send_message( out );
                process_add_to_runqueue( this->current_transfer->requesting_process );
            }
        }
        
        this->current_transfer = (this->current_operation ? this->write_queue : this->read_queue).remove();
        this->transfer_start( this->current_transfer );
        this->currently_idle = false;
    }
}

bool __ata_channel::transfer_available() {
    bool op = this->current_operation;
    if( (op ? this->write_queue : this->read_queue).count() == 0 ) {
        op = !op;
        if( (op ? this->write_queue : this->read_queue).count() == 0 ) {
            return false;
        }
    }
    return true;
}

void __ata_channel::transfer_start( ata_transfer_request *req ) {
    if(req == NULL) {
        return;
    }
    bool is_lba48 = false;
    //kprintf("ata: beginning transfer.\n");
    if( req->dma ) {
        this->prdt_virt[0] = ((uint32_t)req->buffer.buffer_phys);
        this->prdt_virt[1] = (1<<31) | ((uint16_t)(req->n_sectors*512));
    }
    if( req->to_slave ) {
        is_lba48 = this->devices[1].lba48;
    } else {
        is_lba48 = this->devices[0].lba48;
    }
    if( (!is_lba48) && ( req->sector_start > 0x0FFFFFFF) ) {
        kprintf("ata: transfer attempted to access past LBA28 limit! (sector=%llu)\n", req->sector_start);
        return;
    }
    this->wait_bsy();
    
    if( req->dma ) {
        io_inb( this->bus_master );
        io_inb( this->bus_master+2 );
        if( req->read )
            io_outb(this->bus_master, 8);
        else
            io_outb(this->bus_master, 0);
        io_inb( this->bus_master );
    }
    
    // send DMA parameters to drive
    if( is_lba48 ) {
        if(req->to_slave)
            this->select( 0xF0 );
        else
            this->select( 0xE0 );
        //kprintf("ata: sending transfer parameters.\n * n_sectors = %u\n * sector_start(LBA48) = %u\n", req->n_sectors, req->sector_start);
        io_outb( this->base+2, (req->n_sectors>>8)    & 0xFF );
        io_outb( this->base+3, (req->sector_start>>24)& 0xFF );
        io_outb( this->base+4, (req->sector_start>>32)& 0xFF );
        io_outb( this->base+5, (req->sector_start>>40)& 0xFF );
        
        io_outb( this->base+2, req->n_sectors         & 0xFF );
        io_outb( this->base+3, req->sector_start      & 0xFF );
        io_outb( this->base+4, (req->sector_start>>8) & 0xFF );
        io_outb( this->base+5, (req->sector_start>>16)& 0xFF );
        // send DMA command
        //kprintf("ata: sending DMA command.\n");
        while( ((io_inb( this->control ) & ATA_SR_DRDY) == 0) );
        this->error_handled = false;
        if( req->dma ) {
            if( req->read ) {
                //kprintf("ata: sending ATA_CMD_READ_DMA_EXT.\n");
                io_outb( this->base+7, ATA_CMD_READ_DMA_EXT );
            } else {
                //kprintf("ata: sending ATA_CMD_WRITE_DMA_EXT.\n");
                io_outb( this->base+7, ATA_CMD_WRITE_DMA_EXT );
            }
        } else {
            if( req->read ) {
                //kprintf("ata: sending ATA_CMD_READ_PIO_EXT.\n");
                io_outb( this->base+7, ATA_CMD_READ_PIO_EXT );
            } else {
                //kprintf("ata: sending ATA_CMD_WRITE_PIO_EXT.\n");
                io_outb( this->base+7, ATA_CMD_WRITE_PIO_EXT );
            }
        }
    } else {
        if(req->to_slave)
            this->select( 0xF0 | ( (req->sector_start >> 24) & 0x0F ) );
        else
            this->select( 0xE0 | ( (req->sector_start >> 24) & 0x0F ) );
        //kprintf("ata: sending transfer parameters.\n * n_sectors = %u\n * sector_start(LBA28) = %u\n", req->n_sectors, req->sector_start);
        io_outb( this->base+2, req->n_sectors         & 0xFF );
        io_outb( this->base+3, req->sector_start      & 0xFF );
        io_outb( this->base+4, (req->sector_start>>8) & 0xFF );
        io_outb( this->base+5, (req->sector_start>>16)& 0xFF );
        // send DMA command
        //kprintf("ata: sending DMA command.\n");
        while( ((io_inb( this->control ) & ATA_SR_DRDY) == 0) ); // wait on drive ready
        this->error_handled = false;
        if( req->dma ) {
            if( req->read ) {
                //kprintf("ata: sending ATA_CMD_READ_DMA.\n");
                io_outb( this->base+7, ATA_CMD_READ_DMA );
            } else {
                //kprintf("ata: sending ATA_CMD_WRITE_DMA.\n");
                io_outb( this->base+7, ATA_CMD_WRITE_DMA );
            }
        } else {
            if( req->read ) {
                //kprintf("ata: sending ATA_CMD_READ_PIO.\n");
                io_outb( this->base+7, ATA_CMD_READ_PIO );
            } else {
                //kprintf("ata: sending ATA_CMD_WRITE_PIO.\n");
                io_outb( this->base+7, ATA_CMD_WRITE_PIO );
            }
        }
    }
    
    if( !req->dma ) {
        uint16_t* current = (uint16_t*)req->buffer.remap();
        //kprintf("ata: buffer at %#p physical, %#p virtual.\n", req->buffer.buffer_phys, (void*)current);
        //kprintf("ata: PTE for virt address is %#x\n", paging_get_pte((size_t)current));
        for( unsigned int i=0;i<req->n_sectors;i++ ) {
            while( ((io_inb( this->control ) & ATA_SR_BSY) > 0) || ((io_inb( this->control ) & ATA_SR_DRQ) == 0) );
            for(unsigned int j=0;j<256;j++) {
                if( req->read )
                    *current++ = io_inw( this->base );
                else
                    io_outw( this->base, *current++ );
            }
            for(int k=0;k<4;k++) // 400 ns delay
                io_inb( this->control );
        }
        if( !req->read ) {
            if(req->to_slave) {
                this->select( 0x10 );
            } else {
                this->select( 0 );
            }
            while( ((io_inb( this->control ) & ATA_SR_BSY) > 0) || ((io_inb( this->control ) & ATA_SR_DRDY) == 0) );
            if( is_lba48 ) {
                io_outb( this->base+7, ATA_CMD_CACHE_FLUSH_EXT );
            } else {
                io_outb( this->base+7, ATA_CMD_CACHE_FLUSH );
            }
            while( ((io_inb( this->control ) & ATA_SR_BSY) > 0) );
        }
        flushCache(); // flush the (memory) cache cpu-side as well
        //kprintf("ata: PIO transfer complete.\n");
        req->status = true;
        message out("transfer_complete", req, sizeof(ata_transfer_request));
        req->requesting_process->send_message( out );
        k_vmem_free( (size_t)current );
        process_add_to_runqueue( req->requesting_process );
        this->delayed_starter->state = process_state::runnable; // indirectly schedule ourselves to run later
        process_add_to_runqueue(this->delayed_starter);
        
    } else {
        uint8_t current_bmdma_cmd = io_inb( this->bus_master ); // start DMA
        io_outb(this->bus_master, current_bmdma_cmd | 1);
    }
}

void irq14_delayed_starter() {
    while(true) {
        if( ata_channels[0].transfer_available() ) {
            ata_channels[0].transfer_cycle();
        } else {
            process_current->state = process_state::waiting;
            process_switch_immediate();
        }
        //kprintf("IRQ14!\n");
    }
}

void irq15_delayed_starter() {
    while(true) {
        if( ata_channels[1].transfer_available() ) {
            ata_channels[1].transfer_cycle();
        } else {
            process_current->state = process_state::waiting;
            process_switch_immediate();
        }
        //kprintf("IRQ15!\n");
    }
}

void ata_print_error( uint8_t err ) {
    if( err & 0x80 ) kprintf("ata: Bad block mark detected.\n");
    if( err & 0x40 ) kprintf("ata: Uncorrectable data error.\n");
    if( err & 0x20 ) kprintf("ata: No Media / Media Error (MC).\n");
    if( err & 0x10 ) kprintf("ata: Sector ID field could not be found.\n");
    if( err & 0x08 ) kprintf("ata: No Media / Media Error (MCR).\n");
    if( err & 0x04 ) kprintf("ata: Command aborted.\n");
    if( err & 0x02 ) kprintf("ata: Track 0 could not be found.\n");
    if( err & 0x01 ) kprintf("ata: Data address mark not found.\n");
}

void ata_error_checker() {
    uint64_t last_time = 0;
    while(true) {
        if( get_sys_time_counter() >= (last_time+1000) ) {
            uint8_t stat_0 = io_inb( ata_channels[0].control );
            uint8_t stat_1 = io_inb( ata_channels[1].control );
            if( ((stat_0 & 0x80) == 0) && ( ((stat_0 & 1) > 0) || ((stat_0 & 0x20) > 0) ) ) {
                if( !ata_channels[0].error_handled ) {
                    kprintf("ata: Error on channel 0.\n");
                    if( ((stat_0 & 1) > 0) )
                        ata_print_error( io_inb( ata_channels[0].base+1 ) );
                    else if((stat_0 & 0x20) > 0)
                        kprintf("ata: Drive fault.\n");
                    
                    last_time = get_sys_time_counter();
                    ata_channels[0].error_handled = true;
                }
            }
            if( ((stat_1 & 0x80) == 0) && ( ((stat_1 & 1) > 0) || ((stat_1 & 0x20) > 0) ) ) {
                if( !ata_channels[1].error_handled ) {
                    kprintf("ata: Error on channel 1.\n");
                    if( ((stat_1 & 1) > 0) )
                        ata_print_error( io_inb( ata_channels[1].base+1 ) );
                    else if((stat_1 & 0x20) > 0)
                        kprintf("ata: Drive fault.\n");
                        
                    last_time = get_sys_time_counter();
                    ata_channels[1].error_handled = true;
                }
            }
        }
        process_switch_immediate();
    }
}

void ata_dma_poller() { // stop-gap measure until I can get interrupts working
    while(true) {
        uint8_t stat_0 = io_inb( ata_channels[0].bus_master+2 );
        uint8_t cmd_0 = io_inb( ata_channels[0].bus_master );
        
        uint8_t stat_1 = io_inb( ata_channels[1].bus_master+2 );
        uint8_t cmd_1 = io_inb( ata_channels[1].bus_master );
        
        if( !ata_channels[0].currently_idle ) {
            if( ((stat_0 & 0x01) == 0) && ((cmd_0 & 1) == 1) ) {
                kprintf("ata: DMAs complete on channel 0.\n");
                if( (stat_0 & 0x02) ) {
                    kprintf("ata: DMA error on channel 0.\n");
                }
                ata_channels[0].transfer_cycle();
            } else if( ((cmd_0 & 1) == 1) && (stat_0 != 0) ) {
                kprintf("ata: DMAs in progress on channel 0.\n");
            }
        }
        
        if( !ata_channels[1].currently_idle ) {
            if( ((stat_1 & 0x01) == 0) && ((cmd_1 & 1) == 1) ) {
                kprintf("ata: DMAs complete on channel 1.\n");
                if( (stat_1 & 0x02) ) {
                    kprintf("ata: DMA error on channel 1.\n");
                }
                ata_channels[1].transfer_cycle();
            } else if( ((cmd_1 & 1) == 1) && (stat_1 != 0) ) {
                kprintf("ata: DMAs in progress on channel 1.\n");
            }
        }
        process_switch_immediate();
    }
}


bool dma_irq( unsigned int channel_no ) {
    uint8_t status = io_inb( ata_channels[channel_no].bus_master+2 );
    if( status & 4 ) { // did this channel cause the interrupt?
        kprintf("ata: channel %u irq\n", channel_no);
        io_inb( ata_channels[channel_no].base+7 ); // if so, then read regular status register
        ata_channels[channel_no].delayed_starter->state = process_state::runnable;
        process_add_to_runqueue(ata_channels[channel_no].delayed_starter);
        return true;
    }
    return false;
}

bool irq14_handler() { /* kprintf("IRQ14!\n"); */ return dma_irq(0); }
bool irq15_handler() { /* kprintf("IRQ15!\n"); */ return dma_irq(1); }
bool ata_dual_handler() { terminal_writestring("IRQ14!\n"); return ( dma_irq(0) || dma_irq(1) ); }

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
    if( this->devices[0].lba48 ) {
        io_outb( this->base+7, ATA_CMD_CACHE_FLUSH_EXT );
    } else {
        io_outb( this->base+7, ATA_CMD_CACHE_FLUSH );
    }
    this->wait_bsy();
    
    this->select( ATA_SLAVE );
    this->wait_bsy();
    if( this->devices[1].lba48 ) {
        io_outb( this->base+7, ATA_CMD_CACHE_FLUSH_EXT );
    } else {
        io_outb( this->base+7, ATA_CMD_CACHE_FLUSH );
    }
    this->wait_bsy();
}

void ata_device::initialize() {
    this->lba48 = ( (((uint16_t*)this->ident)[83] & 0x400) );
    
    for(unsigned int i=0;i<40;i+=2) {
        this->model[i+1] = ((char*)this->ident)[54+i];
        this->model[i] = ((char*)this->ident)[55+i];
    }
    
    this->model[40] = 0;
    
    for(unsigned int i=0;i<20;i+=2) {
        this->serial[i+1] =  ((char*)this->ident)[20+i];
        this->serial[i] =  ((char*)this->ident)[21+i];
    }
    
    this->serial[20] = 0;
    
    if( this->lba48 ) {
        this->n_sectors = ((uint64_t*)this->ident)[25];
    } else {
        this->n_sectors = ((uint32_t*)this->ident)[30];
    }
}

void __ata_channel::initialize( short base, short control, short bus_master ) {
    this->base = base;
    this->control = control;
    
    // reset control register (both devices)
    this->select( 0x40 );
    io_outb( control, 0 );
    this->select( 0x50 );
    io_outb( control, 0 );
    
    this->bus_master         = bus_master;
    
    // set PRDT addresses
    
    io_outb( this->bus_master+4,  this->prdt_phys      & 0xFF );
    io_outb( this->bus_master+5, (this->prdt_phys>>8)  & 0xFF );
    io_outb( this->bus_master+6, (this->prdt_phys>>16) & 0xFF );
    io_outb( this->bus_master+7, (this->prdt_phys>>24) & 0xFF );
    /*
    io_outd( this->bus_master+4, this->prdt_phys );
    */
    this->do_identify();
    
    this->devices[0].initialize();
    this->devices[1].initialize();
    
    kprintf("    * master: %s\n", this->devices[0].model);
    kprintf("    * %s, %u sectors addressable\n", ( this->devices[0].lba48 ? "LBA48 supported" : "LBA48 not supported" ), this->devices[0].n_sectors);
    kprintf("    * slave: %s\n", this->devices[1].model);
    kprintf("    * %s, %u sectors addressable\n", ( this->devices[1].lba48 ? "LBA48 supported" : "LBA48 not supported" ), this->devices[1].n_sectors);
}

void __ata_controller::initialize() {
    this->primary_channel   = 0x1F0; //this->device->registers[0].raw;
    this->primary_control   = 0x3F6; //this->device->registers[1].raw;
    this->secondary_channel = 0x170; //this->device->registers[2].raw;
    this->secondary_control = 0x376; //this->device->registers[3].raw;
    this->dma_control_base  = this->device->registers[4].raw;
    
    /*
    kprintf("ata: primary channel at %s address %#x.\n", (this->device->registers[0].io_space ? "IO" : "Memory"), this->device->registers[0].raw );
    kprintf("ata: primary control at %s address %#x.\n", (this->device->registers[1].io_space ? "IO" : "Memory"), this->device->registers[1].raw );
    kprintf("ata: secondary channel at %s address %#x.\n", (this->device->registers[2].io_space ? "IO" : "Memory"), this->device->registers[2].raw );
    kprintf("ata: secondary control at %s address %#x.\n", (this->device->registers[3].io_space ? "IO" : "Memory"), this->device->registers[3].raw );
    */
    kprintf("ata: controller BMIDE registers at %s address %#x.\n", (this->device->registers[4].io_space ? "IO" : "Memory"), this->device->registers[4].raw);
    // PCI command register:
    pci_write_config_16( this->device->bus, this->device->device, this->device->func, 0x04, 0x5 ); // Bus Master Enable | I/O Space Enable
    
    // determine IRQs:
    pci_read_config_8( this->device->bus, this->device->device, this->device->func, 0x3C );
    pci_write_config_8( this->device->bus, this->device->device, this->device->func, 0x3C, 0xFE );
    if( pci_read_config_8( this->device->bus, this->device->device, this->device->func, 0x3C ) == 0xFE ) {
        // device needs IRQ assignment (we'll just use IRQ14 in this case)
        kprintf("ata: controller IRQ assignment required\n");
        irq_add_handler(14, (size_t)(&ata_dual_handler));
        pci_write_config_8( this->device->bus, this->device->device, this->device->func, 0x3C, 14 );
        if( pci_read_config_8( this->device->bus, this->device->device, this->device->func, 0x3C ) != 14 ) {
            // write didn't go through?
            kprintf("ata: controller IRQ assignment didn't go through?\n");
        }
    } else {
        // device doesn't need an IRQ assignment, is it PATA?
        if( (this->device->prog_if == 0x8A) || (this->device->prog_if == 0x80) ) {
            // it's PATA, so we just assign IRQs 14 and 15.
            kprintf("ata: controller is PATA, no IRQ assignment required\n");
            irq_add_handler(14, (size_t)(&irq14_handler));
            irq_add_handler(15, (size_t)(&irq15_handler));
        } else {
            // in this case we must be using the APIC (Interrupt Line field is R/O)
            // well, just do nothing in this case (we don't support the APIC yet)
            kprintf("ata: Interrupt Line field is for APIC\n");
        }
    }
    
    uint8_t test_ch0 = io_inb(this->primary_channel+7);
    uint8_t test_ch1 = io_inb(this->secondary_channel+7);
    
    ata_channels[0].channel_no = 0;
    ata_channels[1].channel_no = 1;
    
    if( (test_ch0 != 0xFF) || (test_ch1 != 0xFF) ) {
        page_frame *frame = pageframe_allocate(1);
        size_t prdt_virt = k_vmem_alloc(1);
        if( (frame != NULL) && (prdt_virt != NULL) ) {
            paging_set_pte( prdt_virt, frame->address, 0x81 );
            ata_channels[0].prdt_phys = frame->address;
            ata_channels[0].prdt_virt = (uint32_t*)prdt_virt;
            ata_channels[1].prdt_phys = frame->address+0x500;
            ata_channels[1].prdt_virt = (uint32_t*)(prdt_virt+0x500);
            kprintf("PRDTs located at:\n");
            kprintf(" * virtual 0x%p and 0x%p.\n", ata_channels[0].prdt_virt, ata_channels[1].prdt_virt);
            kprintf(" * physical 0x%x and 0x%x.\n", ata_channels[0].prdt_phys, ata_channels[1].prdt_phys);
        }
    }
    
    if( test_ch0 != 0xFF ) {
        ata_channels[0].initialize( this->primary_channel, this->primary_control, this->device->registers[4].phys_address );
        ata_channels[0].delayed_starter = new process( (size_t)&irq14_delayed_starter, 0, false, "ata_ch0_delayedstarter", NULL, 0 );
        spawn_process( ata_channels[0].delayed_starter );
        
        ata_channels[0].devices_present = true;
        kprintf("ata: devices connected on channel 0 (not floating).\n");
    } else {
        // floating bus, there's nothing here
        kprintf("ata: no devices connected on channel 0.\n");
    }
    
    if( test_ch1 != 0xFF ) {
        ata_channels[1].initialize( this->secondary_channel, this->secondary_control, this->device->registers[4].phys_address+8 );
        ata_channels[1].delayed_starter = new process( (size_t)&irq15_delayed_starter, 0, false, "ata_ch1_delayedstarter", NULL, 0 );
        spawn_process( ata_channels[1].delayed_starter );
        
        ata_channels[0].devices_present = true;
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

#define ___ata_register_io_disk(channel_no, device_no) do { \
    if( ata_channels[channel_no].devices[device_no].present ) { \
        ata_io_disk *disk = new ata_io_disk;\
        disk->channel  = &ata_channels[channel_no];\
        disk->to_slave = (device_no == 1);\
        io_register_disk( disk );\
    } \
} while(0)

void ata_initialize() {
    for(unsigned int i=0;i<pci_devices.count();i++) {
        pci_device *current = pci_devices[i];
        if( (current->class_code == 0x01) && (current->subclass_code == 0x01) ) {
            kprintf("ata: found controller (device ID: %u [%u/%u/%u])\n", i, current->bus, current->device, current->func);
            ata_controller.device = current;
            
            ata_controller.initialize();
            
            err_chk = new process( (size_t)&ata_error_checker, 0, false, "ata_error_checker", NULL, 0 );
            //dma_chk = new process( (size_t)&ata_dma_poller, 0, false, "ata_dma_checker", NULL, 0 );
            
            spawn_process( err_chk );
            //spawn_process( dma_chk );
            
            // register disks
            if( ata_channels[0].devices_present ) {
                ___ata_register_io_disk(0, 0);
                ___ata_register_io_disk(0, 1);
            }
            
            if( ata_channels[1].devices_present ) {
                ___ata_register_io_disk(1, 0);
                ___ata_register_io_disk(1, 1);
            }
            
            return;
        }
    }
}

void ata_start_request( ata_transfer_request* req, unsigned int channel_no ) {
    if( (channel_no != 0) && (channel_no != 1) )
        return;
    ata_channels[channel_no].enqueue_request( req );
}

void ata_do_pio_sector_transfer( int channel, void* buffer, bool write ) {
    uint16_t* current = (uint16_t*)buffer;
    while( ((io_inb( ata_channels[channel].control ) & 0x80) > 0) || ((io_inb( ata_channels[channel].control ) & 0x8) == 0) ); // busy-wait on DRQ
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
