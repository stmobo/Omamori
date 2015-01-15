// ext2.h
#pragma once
#include "includes.h"

struct ext2_superblock {
    uint32_t n_inodes;
    uint32_t n_blocks;
    uint32_t n_reserved_blocks;
    uint32_t n_unallocated_blocks;
    uint32_t n_unallocated_inodes;
    uint32_t superblock_no;
    uint32_t block_sz;
    uint32_t fragment_sz;
    uint32_t blocks_per_group;
    uint32_t fragments_per_group;
    uint32_t inodes_per_group;
    uint32_t last_mount_time;
    uint32_t last_write_time;
    uint16_t last_fsck_mount;
    uint16_t fsck_mount_interval;
    uint16_t signature;
    uint16_t state;
    uint16_t error_recovery;
    uint16_t version_minor;
    uint32_t last_fsck_time;
    uint32_t fsck_time_interval;
    uint32_t os_id;
    uint32_t version_major;
    uint16_t reserved_userid;
    uint16_t reserved_groupid;
} __attribute__((packed));

struct ext2_superblock_extended {
    uint32_t user_inode_start;
    uint16_t inode_sz;
    uint16_t superblock_block_group;
    uint16_t optional_features;
    uint16_t required_features;
    uint16_t readonly_features;
    char fs_id[16];
    char vol_name[16];
    char last_mount_path[64];
    uint32_t compression_used;
    uint8_t block_preallocate_file;
    uint8_t block_preallocate_directory;
    uint16_t unused;
    char journal_id[16];
    uint32_t journal_inode;
    uint32_t journal_device;
    uint32_t journal_inode_list_head;
} __attribute__((packed));

struct ext2_bg_descriptor {
    uint32_t block_bitmap_addr;
    uint32_t inode_bitmap_addr;
    uint32_t inode_table_addr;
    uint16_t n_unallocated_blocks;
    uint16_t n_unallocated_inodes;
    uint16_t n_directories;
    uint8_t  unused[14];
} __attribute__((packed));

struct ext2_inode {
    uint16_t permissions;
    uint16_t user_id;
    uint32_t file_size;
    uint32_t last_access_time;
    uint32_t creation_time;
    uint32_t last_modify_time;
    uint32_t deletion_time;
    uint16_t group_id;
    uint16_t n_hard_links;
    uint32_t n_disk_sectors;
    uint32_t flags;
    uint32_t os_specific_value_1;
    uint32_t direct_ptrs[12];
    uint32_t indirect_ptr;
    uint32_t double_indirect_ptr;
    uint32_t triple_indirect_ptr;
    uint32_t gen_no;
    uint32_t ext_attr_1;
    uint32_t ext_attr_2;
    uint32_t fragment_addr;
    uint32_t os_specific_value_2[3];
} __attribute__((packed));

struct ext2_directory_entry {
    uint32_t inode_address;
    uint16_t entry_sz;
    uint8_t  name_length;
    uint8_t  type_indicator;
    char     name[1]; // length of name is (entry_sz - (4+2+1+1)) = (entry_sz - 8)
} __attribute__((packed));

struct ext2_filesystem {
    unsigned int device_id;
    
    uint8_t superblock_raw[1024];
    ext2_superblock* superblock;
    ext2_superblock_extended* ext_superblock;
    
    uint8_t* blk_group_desc_raw;
    ext2_bg_descriptor* blk_group_desc;
    
    uint32_t block_size;
    uint32_t fragment_size;
    uint32_t inode_size;
    
    void* read_block( uint32_t );
    void  write_block( uint32_t, void* );
    ext2_inode* get_inode( uint32_t );
    
    ext2_filesystem( unsigned int );
};
