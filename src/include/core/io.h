// io.h - Header for io.cpp

#pragma once
#include "includes.h"
#include "core/paging.h"
#include "core/scheduler.h"
#include "core/message.h"

typedef struct transfer_buffer {
    void        *buffer_virt;
    void        *buffer_phys;
    page_frame  *frames;
    unsigned int n_frames;
    size_t       size;
    
    transfer_buffer( unsigned int );
    ~transfer_buffer() { pageframe_deallocate( this->frames, this->n_frames ); };
    void* remap();
} transfer_buffer;


typedef struct transfer_request {
    uint64_t        id;
    transfer_buffer buffer;
    uint64_t        sector_start;
    size_t          n_sectors;
    bool            status = false;
    bool            read;
    process*        requesting_process;
    channel_receiver* ch;
    
    transfer_request( transfer_buffer, uint64_t, size_t, bool );
    transfer_request( transfer_buffer*, uint64_t, size_t, bool );
    transfer_request( transfer_request& );
    ~transfer_request() { delete this->ch; };
    void wait();
} transfer_request;

struct io_disk {
    unsigned int device_id;
    
    virtual void send_request( transfer_request* ) =0;
    virtual unsigned int  get_sector_size() =0;
    virtual unsigned int  get_total_size() =0;
};

struct io_partition {
    unsigned int device;
    unsigned int global_id;
    unsigned int part_id;
    uint32_t start;
    uint32_t size;
    uint8_t  id;
};

extern void io_register_disk( io_disk* );
extern void io_detect_disk( io_disk* dev );
extern io_disk* io_get_disk( unsigned int );
extern io_partition* io_get_partition( unsigned int );
extern io_partition* io_get_partition( unsigned int, unsigned int );
extern unsigned int io_part_ids_to_global( unsigned int, unsigned int );
extern void io_read_partition( unsigned int, void*, uint64_t, uint64_t );
extern void io_read_partition( unsigned int, unsigned int, void*, uint64_t, uint64_t );
extern void io_write_partition( unsigned int, void*, uint64_t, uint64_t );
extern void io_write_partition( unsigned int, unsigned int, void*, uint64_t, uint64_t );
extern unsigned int io_get_disk_count();
extern void io_read_disk(  unsigned int, void*, uint64_t, uint64_t );
extern void io_write_disk( unsigned int, void*, uint64_t, uint64_t );
extern void io_initialize();
