// io.cpp - (Disk) I/O wrapper thing
// really just a standardized way of accessing HDD functions

#include "includes.h"
#include "core/io.h"
#include "core/paging.h"
#include "core/scheduler.h"
#include "lib/refcount.h"

vector<io_disk*> io_disks;
vector<io_partition*> io_partitions;
static uint64_t __io_current_id = 0;

transfer_request::transfer_request( transfer_buffer buf, uint64_t secst, size_t nsec, bool rd ) : id(__io_current_id++), buffer(buf), sector_start(secst), n_sectors(nsec), read(rd), requesting_process(process_current) {};

transfer_request::transfer_request( transfer_buffer *buf, uint64_t secst, size_t nsec, bool rd ) : transfer_request(*buf, secst, nsec, rd) {};

transfer_request::transfer_request( transfer_request& cpy ) : id(cpy.id), buffer(cpy.buffer), sector_start(cpy.sector_start), n_sectors(cpy.n_sectors), read(cpy.read), requesting_process(cpy.requesting_process) {};

void transfer_request::wait() {
    //kprintf("transfer_request::wait - waiting for message (id=%u)\n", this->id);
    bool status = get_message_listen_status( "transfer_complete" );
    set_message_listen_status( "transfer_complete", true );
    while(true) {
        unique_ptr<message> msg = wait_for_message( "transfer_complete" );
        transfer_request* req = (transfer_request*)msg->data;
        if(req != NULL) {
            //kprintf("transfer_request::wait - received valid message (id=%u)\n", req->id);
            if(req->id == this->id) {
                set_message_listen_status( "transfer_complete", status );
                return;
            } else {
                //kprintf("transfer_request::wait - ids did not match\n", req->id);
            }
        } else {
            //kprintf("transfer_request::wait - received invalid message\n");
        }
    }
}

void io_initialize() {
    register_channel( "transfer_complete", CHANNEL_MODE_BROADCAST );
}

void io_register_disk( io_disk *dev ) {
    dev->device_id = (io_disks.count()+1);
    io_disks.add_end( dev );
    
    // read partitions:
    void *buf = kmalloc(512);
    io_read_disk( dev->device_id, buf, 0, 512 );
    if( *((uint16_t*)(((uint32_t)buf)+510)) == 0xAA55 ) {
        kprintf("io: found MBR on disk %u, buf=%#p\n", dev->device_id, buf);
        void *table = (buf+0x1BE);
        for( int i=0;i<4;i++ ) {
            void *cur_entry = (table+(i*16));
            if( *((uint8_t*)(cur_entry+4)) != 0 ) {
                io_partition *part = new io_partition;
                part->id        = *((uint8_t*)(cur_entry+4));
                part->start     = *((uint32_t*)(cur_entry+8));
                part->size      = *((uint32_t*)(cur_entry+12));
                part->device    = dev->device_id;
                part->global_id = io_partitions.count()+1;
                part->part_id   = i;
                io_partitions.add_end(part);
                kprintf("io: read partition %u -- start=%u, size=%u, id=%#x\n", part->global_id, part->start, part->size, part->id);
            }
        }
    } else {
        kprintf("io: found no MBR on disk %u (signature=%#x, buf=%#p)\n", dev->device_id, *((uint16_t*)(buf+0x1FE)), buf);
    }
    /*
    while( true ) {
        asm volatile("hlt");
    }
    */
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

io_partition *io_get_partition( unsigned int id ) {
    for(unsigned int i=0;i<io_partitions.count();i++) {
        if( io_partitions[i]->global_id == id ) {
            return io_partitions[i];
        }
    }
    return NULL;
}

io_partition *io_get_partition( unsigned int device, unsigned int id ) {
    for(unsigned int i=0;i<io_partitions.count();i++) {
        if( (io_partitions[i]->device == device) && (io_partitions[i]->part_id == id) ) {
            return io_partitions[i];
        }
    }
    return NULL;
}

unsigned int io_part_ids_to_global( unsigned int device, unsigned int id ) {
    io_partition *part = io_get_partition(device, id);
    return part->global_id;
}

transfer_buffer::transfer_buffer( unsigned int n_bytes ) {
    this->size = n_bytes;
    this->n_frames = ( (this->size-(this->size%0x1000)) / 0x1000 )+1;
    
    this->frames = pageframe_allocate( this->n_frames );
    this->buffer_virt = (void*)k_vmem_alloc( this->n_frames );
    this->buffer_phys = (void*)this->frames[0].address;
    for(unsigned int i=0;i<this->n_frames;i++) {
        if( !((i <= 0) || (this->frames[i].address == (this->frames[i-1].address+0x1000))) )
            panic("io: Could not allocate contiguous frames for DMA buffer! (i=%#x, i-1=%#x)\n", this->frames[i].address, this->frames[i-1].address);
        paging_set_pte( ((size_t)this->buffer_virt)+(i*0x1000), this->frames[i].address,0x19 );
        //kprintf("transfer_buffer::transfer_buffer: mapping %#x to %#x.\n", ((size_t)this->buffer_virt)+(i*0x1000), this->frames[i].address );
    }
}

void *transfer_buffer::remap() {
    void *buf = (void*)k_vmem_alloc( this->n_frames );
    for(unsigned int i=0;i<this->n_frames;i++) {
        if( !((i <= 0) || (this->frames[i].address == (this->frames[i-1].address+0x1000))) )
            panic("io: Could not allocate contiguous frames for DMA buffer!\n");
        paging_set_pte( ((size_t)buf)+(i*0x1000), this->frames[i].address,0x19 );
        //kprintf("transfer_buffer::remap: mapping %#x to %#x.\n", ((size_t)buf)+(i*0x1000), this->frames[i].address );
    }
    return buf;
}

unsigned int io_get_disk_count() { return io_disks.count(); }

void io_read_disk( unsigned int disk_no, void* out_buffer, uint64_t start_pos, uint64_t read_amt ) {
    io_disk *device = io_get_disk( disk_no );
    //kprintf("io: reading disk %u, position %llu -> %llu (%llu bytes)\n", disk_no, start_pos, start_pos+read_amt, read_amt);
    if( device == NULL ) {
        kprintf("io: attempted read to unknown disk %u\n", disk_no);
        return; // error message?
    }
    bool manual_read_required = ( (read_amt % device->get_sector_size()) != 0 );
    uint64_t n_sectors = ( read_amt / device->get_sector_size() ) + ( manual_read_required ? 1 : 0 );
    uint64_t sector_start = ( start_pos / device->get_sector_size() );
    
    transfer_buffer  *tmp_buffer = new transfer_buffer( n_sectors * device->get_sector_size() );
    transfer_request *req = new transfer_request( tmp_buffer, sector_start, n_sectors, true );
    
    //kprintf("io: sending request for %u sectors from LBA %u\n", n_sectors, sector_start);
    
    device->send_request( req );
    req->wait();
    
    uint8_t *dst_ptr = (uint8_t*)out_buffer;
    uint8_t *src_ptr = (uint8_t*)(tmp_buffer->buffer_virt);
    //kprintf("io: src buffer at %#p physical, %#p virtual\n", tmp_buffer->buffer_phys, (void*)src_ptr);
    //kprintf("io: PTE for source address is %#x\n", paging_get_pte((size_t)src_ptr));
    for( unsigned int i=0;i<read_amt;i++ ) {
        dst_ptr[i] = src_ptr[ i ];
    }
    
    //delete req;
    //delete tmp_buffer;
}

void io_write_disk( unsigned int disk_no, void* out_buffer, uint64_t start_pos, uint64_t write_amt ) {
    io_disk *device = io_get_disk( disk_no );
    if( device == NULL ) {
        kprintf("io: attempted write to unknown disk %u\n", disk_no);
        return; // error message?
    }
    bool manual_read_required = ( (write_amt % device->get_sector_size()) != 0 );
    uint64_t n_sectors = ( write_amt / device->get_sector_size() ) + ( manual_read_required ? 1 : 0 );
    uint64_t sector_start = ( start_pos / device->get_sector_size() );
    
    transfer_buffer  *tmp_buffer = new transfer_buffer( n_sectors * device->get_sector_size() );
    transfer_request *req        = new transfer_request( tmp_buffer, sector_start, n_sectors, false );
    
    uint8_t *src_ptr = (uint8_t*)out_buffer;
    uint8_t *dst_ptr = (uint8_t*)(tmp_buffer->buffer_virt);
    for( unsigned int i=0;i<(write_amt % device->get_sector_size());i++ ) {
        dst_ptr[i] = 0;
    }
    
    for( unsigned int i=0;i<write_amt;i++ ) {
        dst_ptr[ i ] = src_ptr[i];
    }
    
    device->send_request( req );
    
    req->wait();
    
    //delete req;
}

void io_read_partition( unsigned int global_part_id, void *out_buffer, uint64_t start_pos, uint64_t read_amt ) {
    io_partition *part   = io_get_partition(global_part_id);
    io_disk      *device = io_get_disk( part->device );
    if( part == NULL ) {
        kprintf("io: attempted read from unknown partition %u (global)\n", global_part_id);
        return;
    }
    
    // do some sanity checking
    if( (((part->start*device->get_sector_size())+start_pos) + read_amt) > ((part->start*device->get_sector_size())+part->size) ) {
        kprintf("io: attempted read over partition %u boundary", global_part_id);
        return;
    }
    
    io_read_disk( part->device, out_buffer, ((part->start*device->get_sector_size())+start_pos), read_amt );
}

void io_write_partition( unsigned int global_part_id, void *out_buffer, uint64_t start_pos, uint64_t write_amt ) {
    io_partition *part = io_get_partition(global_part_id);
    io_disk      *device = io_get_disk( part->device );
    if( part == NULL ) {
        kprintf("io: attempted write to unknown partition %u (global)\n", global_part_id);
        return;
    }
    
    // do some sanity checking
    if( (((part->start*device->get_sector_size())+start_pos) + write_amt) > ((part->start*device->get_sector_size())+part->size) ) {
        kprintf("io: attempted write over partition %u boundary", global_part_id);
        return;
    }
    
    io_write_disk( part->device, out_buffer, ((part->start*device->get_sector_size())+start_pos), write_amt );
}

void io_read_partition( unsigned int device, unsigned int part_id, void *out_buffer, uint64_t start_pos, uint64_t read_amt ) {
    io_partition *part = io_get_partition(device, part_id);
    if( part == NULL ) {
        kprintf("io: attempted read from unknown partition %u on device %u\n", part_id, device);
        return;
    }
    
    return io_read_partition( part->global_id, out_buffer, start_pos, read_amt );
}

void io_write_partition( unsigned int device, unsigned int part_id, void *out_buffer, uint64_t start_pos, uint64_t write_amt ) {
    io_partition *part = io_get_partition(device, part_id);
    if( part == NULL ) {
        kprintf("io: attempted write to unknown partition %u on device %u\n", part_id, device);
        return;
    }
    
    return io_read_partition( part->global_id, out_buffer, start_pos, write_amt );
}
