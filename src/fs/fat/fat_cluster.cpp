/*
 * fat_cluster.cpp
 *
 *  Created on: Jan 11, 2015
 *      Author: Tatantyler
 */

#include "fs/fat/fat_fs.h"
#include "core/io.h"

uint64_t fat_fs::fat_fs::cluster_to_lba(uint32_t cluster) {
    //return this->first_usable_cluster + cluster * this->sectors_per_cluster - (2*this->sectors_per_cluster);
    return ((cluster-2)*this->params.sectors_per_cluster) + this->params.first_usable_cluster;
}

void * fat_fs::fat_fs::get_cluster( uint32_t cluster ) {
    uint64_t lba = this->cluster_to_lba( cluster );
    void *buf = kmalloc( this->params.sectors_per_cluster * 512 );

    io_read_partition( this->params.part_no, buf, lba*512, this->params.sectors_per_cluster * 512 );
    return buf;
}

void fat_fs::fat_fs::write_cluster( uint32_t cluster, void* data ) {
    uint64_t lba = this->cluster_to_lba( cluster );

    io_write_partition( this->params.part_no, data, lba*512, this->params.sectors_per_cluster * 512 );
}

void * fat_fs::fat_fs::get_clusters( vector<uint32_t> *clusters ) {
    void *buf = kmalloc( clusters->count() * this->params.sectors_per_cluster * 512 );
    void *out = buf;
    uintptr_t out_int = (uintptr_t)buf;
    for( int i=0;i<clusters->count();i++ ) {
        io_read_partition( this->params.part_no, buf, this->cluster_to_lba( clusters->get(i) )*512, this->params.sectors_per_cluster * 512 );
        out_int += (this->params.sectors_per_cluster * 512);
        buf = (void*)out_int;
    }
    return out;
}

unsigned int fat_fs::fat_fs::allocate_cluster() {
    unsigned int current_fat_sector = this->params.n_reserved_sectors; // relative to this->n_reserved_sectors
    unsigned int current_cluster = 2;
    bool do_read = true;
    bool found = false;
    void *buf = kmalloc(512);
    uintptr_t ptr_int = (uintptr_t)buf;

    io_read_partition( this->params.part_no, buf, 512, 512 );

    /*
    uint32_t last_allocated = *((uint32_t*)(ptr_int+492));
    if( last_allocated != 0xFFFFFFFF ) {
        if( last_allocated < this->params.n_clusters ) {
            current_cluster = last_allocated+1;
            current_fat_sector = (this->params.n_reserved_sectors + ( (current_cluster*4) / 512 ));
        }
    }
    */

    memclr(buf, 512);

    do {
        if( do_read ) {
            io_read_partition( this->params.part_no, buf, current_fat_sector*512, 512 );
            do_read = false;
        }

        uint32_t fat_offset = (current_cluster*4) % 512;
        uint8_t* cluster = (uint8_t*)buf;

        uint32_t cluster_entry = *((uint32_t*)(&cluster[fat_offset])) & 0x0FFFFFFF;
        if( cluster_entry == 0 ) {
            found = true;
            unsigned int hi_nibble = (*((uint32_t*)(&cluster[fat_offset])) & 0xF0000000);
            *((uint32_t*)(&cluster[fat_offset])) = hi_nibble | 0x0FFFFFF8;
            io_write_partition( this->params.part_no, buf, current_fat_sector * 512, 512 );

            io_read_partition( this->params.part_no, buf, 512, 512 );

            *((uint32_t*)(ptr_int+492)) = current_cluster;

            io_write_partition( this->params.part_no, buf, 512, 512 );

            break;
        }

        current_cluster++;
        if( current_fat_sector != (this->params.n_reserved_sectors + ( (current_cluster*4) / 512 )) ) {
            current_fat_sector = (this->params.n_reserved_sectors + ( (current_cluster*4) / 512 ));
            do_read = true;
        }
    } while( current_cluster < this->params.n_clusters );

    kfree(buf);

    if( found ) {
    	kprintf("fat_allocate: Allocated cluster %#x\n", current_cluster);
        return current_cluster;
    }
    return 0;
}
