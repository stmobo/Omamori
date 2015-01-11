/*
 * fat_table.cpp
 *
 *  Created on: Jan 11, 2015
 *      Author: Tatantyler
 */

#include "fs/fat/fat_fs.h"
#include "core/io.h"

void fat_fs::fat_fs::write_fat( uint32_t cluster, uint32_t value ) {
    uint32_t fat_sector = this->params.n_reserved_sectors + ( (cluster*4) / 512 );
    uint32_t fat_offset = (cluster*4) % 512;

    void *buf = kmalloc(512);
    uint8_t* cluster_data = (uint8_t*)buf;
    uintptr_t ptr_int = (uintptr_t)buf;

    io_read_partition( this->params.part_no, buf, fat_sector*512, 512 );
    *((uint32_t*)(&cluster_data[fat_offset])) = (*((uint32_t*)(&cluster_data[fat_offset])) & 0xF0000000) | (value & 0x0FFFFFFF);
    io_write_partition( this->params.part_no, buf, fat_sector * 512, 512 );

    kfree(buf);
}

unsigned int fat_fs::fat_fs::read_fat( uint32_t cluster ) {
    uint32_t fat_sector = this->params.n_reserved_sectors + ( (cluster*4) / 512 );
    uint32_t fat_offset = (cluster*4) % 512;

    void *buf = kmalloc(512);
    uint8_t* cluster_data = (uint8_t*)buf;
    uintptr_t ptr_int = (uintptr_t)buf;

    io_read_partition( this->params.part_no, buf, fat_sector*512, 512 );
    uint32_t ret = (*((uint32_t*)(&cluster_data[fat_offset])) & 0x0FFFFFFF);

    kfree(buf);

    return ret;
}
