// Enqueue incoming DMA requests depending on what operation they are (read/write).
// Every time one set of transfers completes, load a new set of transfers of the opposite
// direction, or from the same operation if there are none from the opposite direction.
// Mark ourselves as idle if there are no operations left to complete.

typedef struct dma_request {
    void        *buffer_phys;
    uint64_t     sector_start;
    size_t       n_sectors;
    bool         to_slave;
    bool         status;
};

vector<dma_request*> read_queue;
vector<dma_request*> write_queue;
vector<dma_request*> *selected_operation; // true - reading, false - writing
vector<dma_request*> current_prdt_queue;
bool                 currently_idle;
uint64_t             *prdt;
dma_request*          current_transfer = NULL;

uint32_t             dma_n_prds = 0;
uint32_t             dma_current_prd = 0;

void dma_request_enqueue( dma_request* req, bool read, unsigned int channel_no ) {
    if( read ) {
        read_queue.add(req);
    } else {
        write_queue.add(req);
    }
    if( currently_idle ) {
        if( read ) {
            current_operation = &read_queue;
        } else {
            current_operation = &write_queue;
        }
        dma_fill_prdt( prdt, *current_operation );
        currently_idle = false;
        if( !read )
            io_outb(ata_channels[channel_no].bus_master, 1); // set DMA bit
        else
            io_outb(ata_channels[channel_no].bus_master, 9); // set DMA + write bits
        dma_start( channel_no, true );
    }
}

void dma_fill_prdt( uint64_t* target, vector<dma_request*>& queue ) {
    dma_n_prds = queue.count()-1;
    dma_current_prd = 0;
    if( dma_n_prds < 0xA0 ) {
        for(unsigned int i=0;i<queue.count();i++) {
            dma_request *current = queue.remove();
            prdt[i] = (((uint64_t)(current->n_sectors*512))<<32)+((uint32_t)current->buffer_phys);
            if( queue.count() == 0)
                prdt[i] |= (uint64_t)(1<<63);
            current_prdt_queue.add_end( current );
        }
    } else {
        dma_n_prds = 0xA0;
        for(unsigned int i=0;i<0xA0;i++) {
            dma_request *current = queue.remove();
            prdt[i] = (((uint64_t)(current->n_sectors*512))<<32)+((uint32_t)current->buffer_phys);
            current_prdt_queue.add_end( current );
        }
        prdt[0x98] |= (uint64_t)(1<<63);
    }
}

void dma_swap_operations() {
    if( current_operation == &read_queue ) {
        current_operation = &write_queue;
    } else {
        current_operation = &read_queue;
    }
    if( current_operation->count() <= 0 ) { // if there aren't any transfers on this queue, then check the other one
        if( current_operation == &read_queue ) {
            current_operation = &write_queue;
        } else {
            current_operation = &read_queue;
        }
        if( current_operation->count() <= 0 ) { // if there aren't any on either queue, then mark ourselves as idle
            currently_idle = true;
            current_prdt_queue.clear();
            dma_n_prds = 0;
            dma_current_prd = 0;
        } else
            dma_fill_prdt( prdt, *current_operation );
    } else
        dma_fill_prdt( prdt, *current_operation );
}

void dma_start( unsigned int channel_no, bool no_swap ) {
    if( !no_swap ) {
        dma_current_prd++;
        if( dma_n_prds >= dma_current_prd ) { // no transfers left for the current operation, do other transfers now
            dma_swap_operations();
            if( currently_idle ) { // no transfers at all left
                io_outb(ata_channels[channel_no].bus_master, 0); // terminate DMA mode
                return;
            }
        }
    }
    if( current_transfer != NULL )
        current_transfer->state = true;
    current_transfer = current_prdt_queue.remove();
    if( current_transfer == NULL ) {
        return dma_start(channel_no, no_swap); // swap operations again
    }
    // send DMA parameters to drive
    ata_write( channel_no, ATA_REG_SECCOUNT0, current_transfer->n_sectors&0xFF );
    ata_write( channel_no, ATA_REG_LBA3, (current_transfer->sector_start>>24)&0xFF );
    ata_write( channel_no, ATA_REG_LBA4, (current_transfer->sector_start>>32)&0xFF );
    ata_write( channel_no, ATA_REG_LBA5, (current_transfer->sector_start>>40)&0xFF );
    ata_write( channel_no, ATA_REG_SECCOUNT1, (current_transfer->n_sectors>>8)&0xFF );
    ata_write( channel_no, ATA_REG_LBA0, current_transfer->sector_start&0xFF );
    ata_write( channel_no, ATA_REG_LBA1, (current_transfer->sector_start>>8)&0xFF );
    ata_write( channel_no, ATA_REG_LBA2, (current_transfer->sector_start>>16)&0xFF );
    // send DMA command
    ata_write( channel_no, ATA_REG_COMMAND, ATA_CMD_WRITE_DMA_EXT );
}

bool dma_irq( unsigned int channel_no ) {
    uint8_t status = io_inb( ata_channels[channel_no].bus_master+2 );
    if( status & 4 ) { // did this channel cause the interrupt?
        dma_start(channel_no, false);
        return true;
    }
    return false;
}