// fat.h
#pragma once
#include "includes.h"
#include "core/vfs.h"

struct fat_direntry {
    unsigned char shortname[8];
    unsigned char ext[3];
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
    
    unsigned char name_lo[10];
    uint8_t attr;
    uint8_t type;
    uint8_t checksum;
    
    unsigned char name_med[12];
    uint8_t zero[2];
    
    unsigned char name_hi[4];
} __attribute__((packed));

struct fat_file {
    vector<fat_longname*> name_entries;
    fat_direntry direntry;
    uint32_t cluster;
    fat_file *parent;
    
    fat_file( fat_file* p, fat_direntry *ent ) : direntry(*ent), parent(p) {}
};

struct fat_directory : public fat_file {
    vector<fat_file*> files;
    using fat_file::fat_file;
};

class fat32_fs {
    private:
    uint8_t sector_one[512];
    
    public:
    unsigned int part_no;
    //uint16_t bytes_per_sector;  // offset 11
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
    uint32_t n_data_sectors;
    uint32_t n_clusters;
    
    // constructors
    fat32_fs(unsigned int);
    fat32_fs( unsigned int device_no, unsigned int part_no ) : fat32_fs( io_part_ids_to_global(device_no, part_no) ) {};
    
    // VFS interface functions
    vfs_file* create_file( unsigned char*, vfs_directory* );
    vfs_directory* create_directory( unsigned char*, vfs_directory* );
    void delete_file( vfs_file* );
    void read_file( vfs_file*, void* );
    void write_file( vfs_node*, void*, size_t);
    void copy_file( vfs_file*, vfs_directory* );
    void move_file( vfs_file*, vfs_directory* );
    vfs_directory *read_directory( vfs_directory*, fat_directory*, char* );
    
    // cluster chain manipulation
    vector<uint32_t> *read_cluster_chain( uint32_t start, uint64_t* n_clusters=NULL );
    void write_cluster_chain( uint32_t, void*, size_t);
    void *get_cluster_chain(uint32_t, uint64_t* );
    void extend_chain( uint32_t, uint32_t );
    void shrink_chain( uint32_t, uint32_t );
    
    // FAT read/write
    unsigned int allocate_cluster();
    void write_fat( uint32_t, uint32_t );
    unsigned int read_fat( uint32_t );
    
    // single cluster manipulation functions
    void *get_clusters( vector<uint32_t>* );
    void *get_cluster( uint32_t );
    void write_cluster( uint32_t, void* );
    uint64_t cluster_to_lba(uint32_t);
    
    // utility functions for directories
    fat_direntry* fat32_find_direntry( void*, uint64_t, fat_file* );
    fat_direntry* fat32_find_free_direntry( void*, uint64_t, unsigned int );
    
    void fat32_generate_basisname( unsigned char*, fat_file* );
};

// formatting functions
unsigned int fat32_format_get_clus_size( unsigned int );
unsigned int fat32_format_get_fat_size( unsigned int );
void fat32_do_format( unsigned int );

// long/short/basis-name manipulation functions
uint8_t fat32_shortname_checksum( unsigned char*, unsigned char* );
vector<fat_longname*> *fat32_split_longname( unsigned char*, unsigned char*, unsigned char* );
char* fat32_construct_longname( vector<fat_longname*> );
char* fat32_construct_shortname( fat_direntry* );