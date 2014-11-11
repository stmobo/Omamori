// fat.cpp
#include "includes.h"
#include "core/io.h"
#include "fs/fat.h"
#include "core/vfs.h"

fat32_fs::fat32_fs( unsigned int disk_no ) {
    io_read_disk( disk_no, (void*)(this->sector_one), 0, 512 );
    
    this->disk_no = disk_no;
    //this->bytes_per_sector     = *( (uint16_t*)( (uintptr_t)(sector_one+11) ) );
    this->sectors_per_cluster  = *( (uint8_t*) ( (uintptr_t)(sector_one+13) ) );
    this->n_reserved_sectors   = *( (uint16_t*)( (uintptr_t)(sector_one+14) ) );
    this->n_fats               = *( (uint16_t*)( (uintptr_t)(sector_one+16) ) );
    this->n_directory_entries  = *( (uint16_t*)( (uintptr_t)(sector_one+17) ) );
    if( *( (uint16_t*)( (uintptr_t)(sector_one+19) ) ) == 0 ) {
        this->n_sectors        = *( (uint32_t*)( (uintptr_t)(sector_one+32) ) );
    } else {
        this->n_sectors        = *( (uint16_t*)( (uintptr_t)(sector_one+19) ) );
    }
    this->n_hidden_sectors     = *( (uint32_t*)( (uintptr_t)(sector_one+21) ) );
    this->fat_size_sectors     = *( (uint32_t*)( (uintptr_t)(sector_one+36) ) );
    this->root_cluster         = *( (uint32_t*)( (uintptr_t)(sector_one+44) ) );
    this->fsinfo               = *( (uint16_t*)( (uintptr_t)(sector_one+48) ) );
    this->backup_boot_sector   = *( (uint16_t*)( (uintptr_t)(sector_one+50) ) );
    
    this->first_usable_cluster = this->n_reserved_sectors + (this->n_fats * this->fat_size_sectors);
}

void *fat32_fs::get_cluster( uint32_t cluster ) {
    uint64_t lba = this->cluster_to_lba( cluster );
    void *buf = kmalloc( this->sectors_per_cluster * 512 );
    
    io_read_disk( this->disk_no, buf, lba*512, this->sectors_per_cluster * 512 );
    return buf;
}

void *fat32_fs::get_clusters( vector<uint32_t> *clusters ) {
    void *buf = kmalloc( clusters->count() * this->sectors_per_cluster * 512 );
    void *out = buf;
    for( int i=0;i<clusters->count();i++ ) {
        io_read_disk( this->disk_no, buf, this->cluster_to_lba( clusters->get(i) )*512, this->sectors_per_cluster * 512 );
        buf += (this->sectors_per_cluster * 512);
    }
    return out;
}

vector<uint32_t> *fat32_fs::read_cluster_chain( uint32_t start, uint64_t* n_clusters ) {
    uint32_t current = start;
    uint32_t next = 0;
    vector<uint32_t> *cluster_list = new vector<uint32_t>;
    
    void *buf = kmalloc(512);
    void *buf2 = buf;
    do {
        uint32_t fat_sector = this->n_reserved_sectors + ( ( current * 4 ) / 512 );
        uint32_t fat_offset = (current*4) % 512;
        
        io_read_disk( this->disk_no, buf, fat_sector*512, 512 );
        
        uint8_t* cluster = (uint8_t*)buf;
        
        next = *((uint32_t*)(cluster+fat_offset)) & 0x0FFFFFFF;
        cluster_list->add_end( current );
        
        current = next;
        if( n_clusters != NULL )
            (*n_clusters)++;
    } while( (next != 0) && !( (next & 0x0FFFFFFF) >= 0x0FFFFFF8 ) );
    
    kfree(buf);
    
    return cluster_list;
}

void *fat32_fs::get_cluster_chain( uint32_t start, uint64_t* n_clusters ) {
    vector<uint32_t> *clusters = this->read_cluster_chain( start, n_clusters );
    
    return this->get_clusters( clusters );
}

char *fat32_construct_longname( vector<fat_longname*> name_entries ) {
    char *name = (char*)kmalloc( (name_entries.count() * 13) + 1 );
    char *out = name;
    for(int seq=0;seq<name_entries.count();seq++) {
        for(int i=0;i<name_entries.count();i++) {
            if( name_entries[i]->seq_num == seq ) {
                for(int j=0;j<10;j+=2) {
                    if( (name_entries[i]->name_lo[j] == 0) || (name_entries[i]->name_lo[j] == 0xFF) )
                        break;
                    *name = name_entries[i]->name_lo[j];
                    name++;
                }
                for(int j=0;j<12;j+=2) {
                    if( (name_entries[i]->name_med[j] == 0) || (name_entries[i]->name_med[j] == 0xFF) )
                        break;
                    *name = name_entries[i]->name_med[j];
                    name++;
                }
                for(int j=0;j<4;j+=2) {
                    if( (name_entries[i]->name_hi[j] == 0) || (name_entries[i]->name_hi[j] == 0xFF) ) {
                        if( seq == (name_entries.count()-1) ) {
                            *name = '\0';
                            name++;
                        }
                        break;
                    }
                    *name = name_entries[i]->name_hi[j];
                    name++;
                }
            }
        }
    }
    return out;
}

vfs_directory *fat32_fs::read_directory( uint32_t cluster ) {
    if( cluster == 0 )
        cluster = this->root_cluster;
        
    uint64_t n_clusters = 0;
    void *data = this->get_cluster_chain( cluster, &n_clusters );
    fat_direntry* dir = (fat_direntry*)data;
    fat_file *cur_file = new fat_file;
    vfs_directory *vfs = new vfs_directory;

    for(int i=0;i<n_clusters;i++) {
        for(int j=0;j<(this->sectors_per_cluster*16);j++) { // (this->sectors_per_cluster*512) / 32 = (this->sectors_per_cluster*16)
            // iterate 16 times for each sector in this cluster
            if( dir->attr == 0x0F ) { // long name entry
                fat_longname *ent = new fat_longname;
                memcpy( ent, (fat_longname*)dir, sizeof(fat_longname) );
                if( ent->seq_num & 0x40 )
                    ent->seq_num &= ~(0x40);
                cur_file->name_entries.add_end( ent );
            } else {
                cur_file->direntry = *dir;
                
                vfs_file *file = new vfs_file;
                file->attr.read_only = ( dir->attr & 1 );
                file->attr.hidden    = ( dir->attr & 2 );
                file->fs_data = (void*)cur_file;
                file->name = fat32_construct_longname( cur_file->name_entries );
                
                vfs->files.add( file );
            }
            dir++;
        }
    }
    return vfs;
}
