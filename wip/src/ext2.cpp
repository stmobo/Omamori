// ext2.cpp
#include "includes.h"
#include "core/io.h"
#include "fs/ext2.h"

void* ext2_filesystem::read_block( uint32_t block_addr ) {
    if( this->superblock->n_blocks >= block_addr ) {
        void* block = kmalloc( this->block_size );
        if( block != NULL ) {
            io_read_disk( this->device_id, block, (block_addr*(this->block_size)), this->block_size );
        }
        return block;
    }
    return NULL;
}

void ext2_filesystem::write_block( uint32_t block_addr, void *data ) {
    if( this->superblock->n_blocks >= block_addr ) {
        io_write_disk( this->device_id, data, (block_addr*(this->block_size)), this->block_size );
    }
}

ext2_inode* ext2_filesystem::get_inode( uint32_t inode_no ) {
    unsigned int blk_group = (inode_no-1) / (this->superblock->inodes_per_group);
    uint32_t index = (inode_no-1) % (this->superblock->inodes_per_group);
    uint32_t blk_indx;
    uint32_t blk_addr;
    void*    raw_data;
    void*    blk;
    
    blk_indx = (index*(this->inode_sz)) / this->block_size;
    raw_data = kmalloc( this->inode_sz );
    blk_addr = (this->blk_group_desc[blk_group].inode_table_addr) + blk_indx;
    
    blk = this->read_block( blk_addr );
    
    // inodes_per_block = (this->block_size / this->ext_superblock->inode_sz)
    
    // I'm going to assume that inode_size is a multiple of block_size
    // and that inodes are always aligned to block boundaries (i.e they don't straddle block boundaries)
    uint32_t inode_intrablock_index = (inode_no-1) % (this->block_size / this->inode_sz);
    
    memcpy( (void*)((uint32_t)blk)+((this->inode_sz)*inode_intrablock_index), raw_data, this->inode_sz );
    
    kfree(blk);
    return (ext2_inode*)(raw_data);
}

void ext2_filesystem::set_inode( uint32_t inode_no, ext2_inode* inode_data ) {
    unsigned int blk_group = (inode_no-1) / (this->superblock->inodes_per_group);
    uint32_t index = (inode_no-1) % (this->superblock->inodes_per_group);
    uint32_t blk_indx;
    uint32_t blk_addr;
    void*    blk;
    
    blk_indx = (index*(this->inode_sz)) / this->block_size;
    blk_addr = (this->blk_group_desc[blk_group].inode_table_addr) + blk_indx;
    
    blk = this->read_block( blk_addr );
    
    // inodes_per_block = (this->block_size / this->ext_superblock->inode_sz)
    
    // I'm going to assume that inode_size is a multiple of block_size
    // and that inodes are always aligned to block boundaries (i.e they don't straddle block boundaries)
    uint32_t inode_intrablock_index = (inode_no-1) % (this->block_size / this->inode_sz);
    
    memcpy( (void*)inode_data, (void*)((uint32_t)blk)+((this->inode_sz)*inode_intrablock_index), this->inode_sz );
    
    this->write_block( blk_addr, blk );
    
    kfree(blk);
}

ext2_filesystem::ext2_filesystem( unsigned int deviceID ) {
    if( io_get_disk( deviceID ) != NULL ) {
        this->device_id = deviceID;
        
        io_read_disk( deviceID, this->superblock_raw, 1024, 1024 );
        this->superblock = (ext2_superblock*)this->superblock_raw;
        
        if( this->superblock->signature == 0xEF53 ) {
            this->block_size = (0x400<<(this->superblock->block_sz));
            this->fragment_size = (0x400<<(this->superblock->fragment_sz));
            
            if( this->superblock->version_major >= 1 ) {
                this->ext_superblock = (ext2_superblock_extended*)((uint32_t)this->superblock_raw+84);
                this->inode_size = (this->ext_superblock->inode_sz);
            } else {
                this->ext_superblock = NULL;
                this->inode_size = 128;
            }
            
            if( this->block_size == 1024 ) {
                this->blk_group_desc_raw = this->read_block( 2 );
            } else {
                this->blk_group_desc_raw = this->read_block( 1 );
            }
            
            this->blk_group_desc = (ext2_bg_descriptor*)(this->blk_group_desc_raw);
        } else {
            kprintf("ext2: Device %u does not have an ext2 filesystem.\n", deviceID);
        }
    }
}

void ext2_format( unsigned int deviceID, uint32_t blk_sz_multiple, uint32_t blocks_per_group, unsigned char reserved_percent  ) {
    void* superblock_data = kmalloc(1024);
    ext2_superblock* superblock = (ext2_superblock*)superblock_data;
    io_disk *device = io_get_disk( deviceID );
    uint32_t block_size = 1024<<blk_sz_multiple;
    
    if( device ) {
        // we'll create 1 inode per 14 blocks
        superblock->n_blocks  = ( device->get_total_size() / block_size  );
        unsigned int n_groups = ( superblock->n_blocks / blocks_per_group );
        
        superblock->n_inodes  = ( superblock->n_blocks / 14 );
        superblock->n_inodes += (n_groups - ( superblock->n_inodes % n_groups ));
        
        superblock->n_unallocated_inodes = superblock->n_inodes-1;
        superblock->inodes_per_group = ( superblock->n_inodes / n_groups );
        superblock->blocks_per_group = blocks_per_group;
        if( blk_sz_multiple == 0 ) {
            superblock->superblock_no = 1;
        } else {
            superblock->superblock_no = 0;
        }
        
        superblock->signature = 0xEF53;
        
        superblock->n_reserved_blocks = (uint32_t)( (double)superblock->n_blocks * (double)((double)reserved_percent/100) );
        
        io_write_disk( deviceID, superblock_data, 1024, 1024 );
        
        // create the BGDT
        
        // for every block group from 0 to n_groups (where the block group no. is "i"):
        // (i*size)+1: Superblock copy
        // (i*size)+2: BGDT copy
        // (i*size)+3: Block bitmap
        // (i*size)+4: Inode bitmap
        // (i*size)+5+(inodes_per_group * (block_size / inode_size)):   End of Inode table
        // (i*(size+1)): Last Data Block
        // (i*(size+1))+1: Start of next block
        void *null_data = kmalloc( block_size );
        void *bgdt_raw = kmalloc( block_size );
        ext2_bg_descriptor* bgdt = (ext2_bg_descriptor*)bgdt_raw;
        
        for( unsigned int i=0;i<n_groups;i++) {
            uint32_t start_block = (blocks_per_group*i);
            uint32_t end_block = (blocks_per_group*(i+1));
            bgdt[i].block_bitmap_addr = start_block+3;
            bgdt[i].inode_bitmap_addr = start_block+4;
            bgdt[i].inode_table_addr = start_block+5;
            bgdt[i].n_unallocated_blocks = blocks_per_group;
            bgdt[i].n_unallocated_inodes = (superblock->inodes_per_group / n_groups);
            bgdt[i].n_directories = 0;
        }
        
        // now go write all this to disk
        for( unsigned int i=0;i<n_groups;i++) {
            uint32_t start_block = (blocks_per_group*i);
            io_write_disk( deviceID, superblock_data, (block_size*start_block)+1, block_size );
            io_write_disk( deviceID, bgdt_raw,        (block_size*start_block)+2, block_size );
            io_write_disk( deviceID, null_data,       (block_size*start_block)+3, block_size );
            io_write_disk( deviceID, null_data,       (block_size*start_block)+4, block_size );
        }
    }
}
