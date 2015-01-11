/*
 * fat32_format.cpp
 *
 *  Created on: Jan 11, 2015
 *      Author: Tatantyler
 */
#include "includes.h"
#include "core/io.h"

// courtesy of the fatgen103 document
// assumes that reserved_sector_count=32
// and that n_fats = 2
// and that type_of_fat = fat32
// and that bytes_per_sector = 512
unsigned int fat32_format_get_clus_size( unsigned int n_sectors ) {
    if( n_sectors <= 66600 ) { // 32.5 MB
        return 0; // invalid
    } else if( n_sectors <= 532480 ) { // 260 MB
        return 1; // .5k clusters
    } else if( n_sectors <= 16777216 ) { // 8 GB
        return 8;
    } else if( n_sectors <= 33554432 ) { // 16 GB
        return 16;
    } else if( n_sectors <= 67108864 ) { // 32 GB
        return 32;
    } else { // >32 GB
        return 64;
    }
}

// root_ent_cnt = 0
// bytes_per_sector = 512
// disk_size = n_sectors
// num_fats = 2
// fat_type = fat32
unsigned int fat32_format_get_fat_size( unsigned int n_sectors ) {
    //unsigned int tmp1 = n_sectors - 32;
    //unsigned int tmp2 = 65537; // (( 256*512 ) + 2) / 2;
    return ((n_sectors - 32) + 65536) / 65537; // (tmp1 + (tmp2-1) / tmp2
}

void fat32_do_format( unsigned int part_id ) {
    io_partition* part = io_get_partition( part_id );
    if( part == NULL ) {
        kprintf("fat32: attempted to format nonexistent partition!\n");
        return;
    }

    unsigned int cluster_size = fat32_format_get_clus_size( part->size );
    unsigned int fat_size = fat32_format_get_fat_size( part->size );

    unsigned int n_data_sectors = part->size - ( 32 + (2 * fat_size) );
    unsigned int n_clusters = n_data_sectors / cluster_size;

    if( cluster_size == 0 ) {
        kprintf("fat32: attempted to format invalid partition (too small)!\n");
        return;
    }

    void *bpb = kmalloc(512);
    uintptr_t ptr_int = (uintptr_t)bpb;

    // header: JMP SHORT 3C NOP
    *((uint8_t*)(ptr_int)) = 0xEB;
    *((uint8_t*)(ptr_int+1)) = 0x3C;
    *((uint8_t*)(ptr_int+2)) = 0x90;

    // OEM identifier
    *((char*)(ptr_int+3)) = 'm';
    *((char*)(ptr_int+4)) = 'k';
    *((char*)(ptr_int+5)) = 'd';
    *((char*)(ptr_int+6)) = 'o';
    *((char*)(ptr_int+7)) = 's';
    *((char*)(ptr_int+8)) = 'f';
    *((char*)(ptr_int+9)) = 's';
    *((char*)(ptr_int+10)) = 0;

    // bytes per sector
    *((uint16_t*)(ptr_int+11)) = 512;

    // sectors per cluster
    *((uint8_t*)(ptr_int+13)) = cluster_size;

    // number of reserved sectors
    *((uint16_t*)(ptr_int+14)) = 32;

    // number of FATs
    *((uint8_t*)(ptr_int+16)) = 2;

    // number of root directory entries
    *((uint16_t*)(ptr_int+17)) = 0;

    // number of sectors total (if <= 65535)
    if( part->size <= 65535 ) {
        *((uint16_t*)(ptr_int+19)) = (uint16_t)(part->size);
    } else {
        *((uint16_t*)(ptr_int+19)) = 0;
    }

    // media byte
    *((uint8_t*)(ptr_int+21)) = 0xF8;

    // FAT16 FAT Size (zeroed out for FAT32)
    *((uint16_t*)(ptr_int+22)) = 0;

    // Sectors per track (i think we can safely disregard this)
    *((uint16_t*)(ptr_int+24)) = 0;

    // Number of heads (again, i think we can disregard this)
    *((uint16_t*)(ptr_int+26)) = 0;

    // number of hidden sectors (or, the LBA of the start of the partition)
    *((uint32_t*)(ptr_int+28)) = part->start;

    // number of sectors (32 bit)
    *((uint32_t*)(ptr_int+32)) = part->size;

    // FAT32-BPB: FAT Size (32 bit)
    *((uint32_t*)(ptr_int+36)) = fat_size;

    // FAT32-BPB: Flags
    *((uint16_t*)(ptr_int+40)) = 0;

    // FAT32-BPB: Version number (just set it to zero for now)
    *((uint16_t*)(ptr_int+42)) = 0;

    // FAT32-BPB: Root cluster number
    *((uint32_t*)(ptr_int+44)) = 2;

    // FAT32-BPB: FS Info sector number
    *((uint16_t*)(ptr_int+48)) = 1;

    // FAT32-BPB: Backup boot sector number
    *((uint16_t*)(ptr_int+50)) = 6;

    // reserved bytes
    for(int i=0;i<12;i++) {
        *((uint8_t*)(ptr_int+52+i)) = 0;
    }

    // FAT32-BPB: drive number
    *((uint8_t*)(ptr_int+64)) = 0x80;

    // FAT32-BPB: reserved
    *((uint8_t*)(ptr_int+65)) = 0;

    // FAT32-BPB: signature
    *((uint8_t*)(ptr_int+66)) = 0x29;

    // FAT32-BPB: volume serial number (I'll just ignore this one.)
    *((uint32_t*)(ptr_int+67)) = 0;

    // FAT32-BPB: Volume label string (i'll just set this to "no name")
    *((char*)(ptr_int+71)) = 'N';
    *((char*)(ptr_int+72)) = 'O';
    *((char*)(ptr_int+73)) = ' ';
    *((char*)(ptr_int+74)) = 'N';
    *((char*)(ptr_int+75)) = 'A';
    *((char*)(ptr_int+76)) = 'M';
    *((char*)(ptr_int+77)) = 'E';
    *((char*)(ptr_int+78)) = ' ';
    *((char*)(ptr_int+79)) = ' ';
    *((char*)(ptr_int+80)) = ' ';
    *((char*)(ptr_int+81)) = ' ';

    // FAT32-BPB: System identifier string
    *((char*)(ptr_int+81)) = 'F';
    *((char*)(ptr_int+82)) = 'A';
    *((char*)(ptr_int+83)) = 'T';
    *((char*)(ptr_int+84)) = '3';
    *((char*)(ptr_int+85)) = '2';
    *((char*)(ptr_int+86)) = ' ';
    *((char*)(ptr_int+87)) = ' ';
    *((char*)(ptr_int+88)) = ' ';

    // zero out the rest of the sector:
    for( int i=89;i<512;i++) {
        *((uint8_t*)(ptr_int+i)) = 0;
    }

    io_write_partition( part_id, bpb, 0, 512 );

    void* fsinfo = kmalloc(512);
    ptr_int = (uintptr_t)fsinfo;

    for(int i=0;i<512;i++) {
        *((uint8_t*)(ptr_int+i)) = 0;
    }

    // FSInfo signature
    *((uint32_t*)(ptr_int)) = 0x41615252;

    // FSInfo signature 2
    *((uint32_t*)(ptr_int+484)) = 0x61417272;

    // Free cluster count (all clusters are free except for cluster 2, the root dir)
    *((uint32_t*)(ptr_int+488)) = n_clusters-1;

    // Last allocated cluster (set to 0xFFFFFFFF for now, since we haven't allocated any clusters besides cluster 2)
    *((uint32_t*)(ptr_int+492)) = 0xFFFFFFFF;

    // Trailing signature
    *((uint32_t*)(ptr_int+508)) = 0xAA550000;
    io_write_partition( part_id, fsinfo, 512, 512 );

    // write out backup bootsector and fsinfo sector
    io_write_partition( part_id, bpb, 6*512, 512 );
    io_write_partition( part_id, fsinfo, 7*512, 512 );

    kfree(bpb);
    kfree(fsinfo);

    // write out first FAT (allocate cluster 2):

    uint32_t fat_sector = 32 + ( (2*4) / 512 );
    uint32_t fat_offset = (2*4) % 512;

    void *buf = kmalloc(512);
    uint8_t* cluster_data = (uint8_t*)buf;
    ptr_int = (uintptr_t)buf;

    //io_read_partition( this->part_no, buf, fat_sector*512, 512 );
    *((uint32_t*)(&cluster_data[fat_offset])) = 0x0FFFFFF8;
    io_write_partition( part_id, buf, fat_sector * 512, 512 );

    kfree(buf);
    delete part;
}
