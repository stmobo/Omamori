// fat.cpp
#include "includes.h"
#include "core/io.h"
#include "fs/fat.h"

fat32_fs::fat32_fs( unsigned int disk_no ) {
    io_read_disk( disk_no, (void*)(this->sector_one), 0, 512 );

    this->bytes_per_sector = *((uint16_t*)(((uintptr_t)(this->sector_one))+11));
    this->sectors_per_cluster = *((uint8_t*)(((uintptr_t)(this->sector_one))+13));
    this->n_reserved_sectors = *((uint16_t*)(((uintptr_t)(this->sector_one))+14));
    this->n_file_alloc_tables = *((uint8_t*)(((uintptr_t)(this->sector_one))+16));
    this->n_directory_entries = *((uint16_t*)(((uintptr_t)(this->sector_one))+17));
    if( *((uint16_t*)(((uint16_t)(this->sector_one))+19)) > 0 )
        this->n_sectors = *((uint16_t*)(((uintptr_t)(this->sector_one))+19));
    else
        this->n_sectors = *((uint32_t*)(((uintptr_t)(this->sector_one))+32));
    this->n_hidden_sectors = *((uint32_t*)(((uintptr_t)(this->sector_one))+28));
}