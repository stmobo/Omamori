// fat.h
#pragma once
#include "includes.h"
#include "core/vfs.h"

class fat32_fs {
    private:
    uint8_t sector_one[512];
    
    public:
    unsigned int disk_no;
    //uint16_t bytes_per_sector;    // offset 11
    uint8_t sectors_per_cluster;  // offset 13
    uint16_t n_reserved_sectors;  // offset 14
    uint16_t n_fats;              // offset 16
    uint16_t n_directory_entries; // offset 17
    uint32_t n_sectors;           // offset 19 (or offset 32 if offset 19 == 0)
    uint32_t n_hidden_sectors;    // offset 21
    uint32_t fat_size_sectors;    // offset 36
    uint32_t root_cluster;        // offset 44
    uint16_t fsinfo;              // offset 48
    uint16_t backup_boot_sector;  // offset 50
    uint32_t first_usable_cluster;
    
    fat32_fs(unsigned int);
    vfs_directory *read_directory( uint32_t );
    vector<uint32_t> *read_cluster_chain( uint32_t start, uint64_t* n_clusters=NULL );
    void *get_cluster_chain(uint32_t, uint64_t* );
    void *get_clusters( vector<uint32_t>* );
    void *get_cluster( uint32_t );
    uint64_t cluster_to_lba(uint32_t cluster) { return this->first_usable_cluster + (cluster*this->sectors_per_cluster) - (2*this->sectors_per_cluster); }
};

struct fat_direntry {
    char shortname[8];
    char ext[3];
    uint8_t attr;
    uint8_t nt_reserved;
    
    uint8_t ctime_tenths;
    uint16_t ctime;
    uint16_t cdate;
    uint16_t adate;
    
    uint16_t start_cluster_hi;
    uint16_t wtime;
    uint16_t wdate;
    uint16_t start_cluster_lo;
    
    uint32_t fsize;
} __attribute__((packed));

struct fat_longname {
    uint8_t seq_num;
    
    uint8_t name_lo[10];
    uint8_t attr;
    uint8_t type;
    uint8_t checksum;
    
    uint8_t name_med[12];
    uint8_t zero[2];
    
    uint8_t name_hi[4];
} __attribute__((packed));

struct fat_file {
    vector<fat_longname*> name_entries;
    fat_direntry direntry;
};