// io.h - Header for io.cpp

#pragma once
#include "includes.h"
#include "core/paging.h"
#include "core/scheduler.h"

typedef struct transfer_buffer {
    void        *buffer_virt;
    void        *buffer_phys;
    page_frame  *frames;
    unsigned int n_frames;
    size_t       size;
    
    transfer_buffer( unsigned int );
} transfer_buffer;


typedef struct transfer_request {
    transfer_buffer buffer;
    uint64_t        sector_start;
    size_t          n_sectors;
    bool            status = false;
    bool            read;
    process*        requesting_process;
    
    transfer_request( transfer_buffer buf, uint64_t secst, size_t nsec, bool rd ) : buffer(buf), sector_start(secst), n_sectors(nsec), read(rd), requesting_process(process_current) {};
    transfer_request( transfer_buffer *buf, uint64_t secst, size_t nsec, bool rd ) : buffer(*buf), sector_start(secst), n_sectors(nsec), read(rd), requesting_process(process_current) {};
    transfer_request( transfer_request& cpy ) : buffer(cpy.buffer), sector_start(cpy.sector_start), n_sectors(cpy.n_sectors), read(cpy.read), requesting_process(cpy.requesting_process) {};
    void wait() { while(!this->status) { process_switch_immediate(); } };
} transfer_request;

struct io_disk {
    unsigned int device_id;
    
    virtual void send_request( transfer_request* ) =0;
    virtual unsigned int  get_sector_size() =0;
    virtual unsigned int  get_total_size() =0;
};

struct io_partition {
    unsigned int device;
    unsigned int part_id;
    uint32_t start;
    uint32_t size;
    uint8_t  id;
};

extern void io_register_disk( io_disk* );
extern io_disk* io_get_disk( unsigned int );
extern unsigned int io_get_disk_count();
extern void io_read_disk(  unsigned int, void*, uint64_t, uint64_t );
extern void io_write_disk( unsigned int, void*, uint64_t, uint64_t );