// io.cpp - (Disk) I/O wrapper thing
// really just a standardized way of accessing HDD functions

#include "includes.h"
#include "core/io.h"
#include "core/paging.h"
#include "core/scheduler.h"

vector<io_disk*> io_disks;
vector<io_partition*> io_partitions;

void io_register_disk( io_disk *dev ) {
    dev->device_id = (io_disks.count()+1);
    io_disks.add_end( dev );
    
    // read partitions:
    void *buf = kmalloc(512);
    if( *((uint16_t*)(buf+0x1FE)) == 0xAA55 ) {
        kprintf("io: found MBR on disk %u\n", dev->device_id);
        void *table = (buf+0x1BE);
        for( int i=0;i<4;i++ ) {
            void *cur_entry = (table+(i*16));
            if( *((uint8_t*)(cur_entry+4)) != 0 ) {
                io_partition *part = new io_partition;
                part->device  = dev->device_id;
                part->id      = *((uint8_t*)(cur_entry+4));
                part->start   = *((uint32_t*)(cur_entry+8));
                part->size    = *((uint32_t*)(cur_entry+12));
                part->part_id = io_partitions.count()+1;
                io_partitions.add_end(part);
                kprintf("io: read partition %u -- start=%u, size=%u, id=%#x\n", part->part_id, part->start, part->size, part->id);
            }
        }
    } else {
        kprintf("io: found no MBR on disk %u (signature=%#x)\n", dev->device_id, *((uint16_t*)(buf+0x1FE)));
    }
    kfree(buf);
}

io_disk *io_get_disk( unsigned int id ) {
    for(unsigned int i=0;i<io_disks.count();i++) {
        if( io_disks[i]->device_id == id ) {
            return io_disks[i];
        }
    }
    return NULL;
}

transfer_buffer::transfer_buffer( unsigned int n_bytes ) {
    this->size = n_bytes;
    this->n_frames = ( (this->size-(this->size%0x1000)) / 0x1000 )+1;
    
    this->frames = pageframe_allocate( this->n_frames );
    this->buffer_virt = (void*)k_vmem_alloc( n_frames );
    this->buffer_phys = (void*)this->frames[0].address;
    for(unsigned int i=0;i<this->n_frames;i++) {
        if( !((i <= 0) || (this->frames[i].address == (this->frames[i-1].address+0x1000))) )
            panic("io: Could not allocate contiguous frames for DMA buffer!\n");
        paging_set_pte( ((size_t)this->buffer_virt)+(i*0x1000), this->frames[i].address,0x81 );
    }
};

unsigned int io_get_disk_count() { return io_disks.count(); }

unsigned int io_get_disk_size( unsigned int disk_no ) {
    io_disk* disk = io_get_disk( disk_no );
    if( disk != NULL ) {
        return disk->get_total_size();
    }
    return 0;
}

void io_read_disk( unsigned int disk_no, void* out_buffer, uint64_t start_pos, uint64_t read_amt ) {
    io_disk *device = io_get_disk( disk_no );
    if( device == NULL ) {
        return; // error message?
    }
    bool manual_read_required = ( (read_amt % device->get_sector_size()) != 0 );
    uint64_t n_sectors = ( read_amt / device->get_sector_size() ) + ( manual_read_required ? 0 : 1 );
    uint64_t sector_start = ( start_pos / device->get_sector_size() );
    
    transfer_buffer  *tmp_buffer = new transfer_buffer( n_sectors * device->get_sector_size() );
    transfer_request *req = new transfer_request( tmp_buffer, sector_start, n_sectors, true );
    
    device->send_request( req );
    
    uint8_t *dst_ptr = (uint8_t*)out_buffer;
    uint8_t *src_ptr = (uint8_t*)(tmp_buffer->buffer_virt);
    for( unsigned int i=0;i<read_amt;i++ ) {
        dst_ptr[i] = src_ptr[ (read_amt % device->get_sector_size())+i ];
    }
    
    delete req;
    delete tmp_buffer;
}

void io_write_disk( unsigned int disk_no, void* out_buffer, uint64_t start_pos, uint64_t write_amt ) {
    io_disk *device = io_get_disk( disk_no );
    if( device == NULL ) {
        return; // error message?
    }
    bool manual_read_required = ( (write_amt % device->get_sector_size()) != 0 );
    uint64_t n_sectors = ( write_amt / device->get_sector_size() ) + ( manual_read_required ? 0 : 1 );
    uint64_t sector_start = ( start_pos / device->get_sector_size() );
    
    transfer_buffer  *tmp_buffer = new transfer_buffer( n_sectors * device->get_sector_size() );
    transfer_request *req        = new transfer_request( tmp_buffer, sector_start, n_sectors, false );
    
    uint8_t *src_ptr = (uint8_t*)out_buffer;
    uint8_t *dst_ptr = (uint8_t*)(tmp_buffer->buffer_virt);
    for( unsigned int i=0;i<(write_amt % device->get_sector_size());i++ ) {
        dst_ptr[i] = 0;
    }
    
    for( unsigned int i=0;i<write_amt;i++ ) {
        dst_ptr[ (write_amt % device->get_sector_size())+i ] = src_ptr[i];
    }
    
    device->send_request( req );
    
    req->wait();
    
    delete req;
}