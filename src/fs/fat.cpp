// fat.cpp
#include "includes.h"
#include "core/io.h"
#include "fs/fat.h"
#include "core/vfs.h"

fat32_fs::fat32_fs( unsigned int part_no ) {
    io_read_partition( part_no, (void*)(this->sector_one), 0, 512 );
    
    this->part_no = part_no;
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
    this->n_hidden_sectors     = *( (uint32_t*)( (uintptr_t)(sector_one+28) ) );
    this->fat_size_sectors     = *( (uint32_t*)( (uintptr_t)(sector_one+36) ) );
    this->root_cluster         = *( (uint32_t*)( (uintptr_t)(sector_one+44) ) );
    this->fsinfo               = *( (uint16_t*)( (uintptr_t)(sector_one+48) ) );
    this->backup_boot_sector   = *( (uint16_t*)( (uintptr_t)(sector_one+50) ) );
    
    this->first_usable_cluster = this->n_reserved_sectors + (this->n_fats * this->fat_size_sectors);
    this->n_data_sectors = this->n_sectors - ( this->n_reserved_sectors + (this->n_fats * this->fat_size_sectors) );
    this->n_clusters = this->n_data_sectors / this->sectors_per_cluster;
    
    kprintf("fat32: reading partition %u as FAT32\n", part_no);
    kprintf("fat32: %u sectors per cluster\n", this->sectors_per_cluster);
    kprintf("fat32: %u FATs\n", this->n_fats);
    kprintf("fat32: %u directory entries\n", this->n_directory_entries);
    kprintf("fat32: %u hidden sectors\n", this->n_hidden_sectors);
    kprintf("fat32: %u reserved sectors\n", this->n_reserved_sectors);
    kprintf("fat32: %u sectors per FAT\n", this->fat_size_sectors);
    kprintf("fat32: root cluster located at cluster %u\n", this->root_cluster);
    kprintf("fat32: data located at %#p\n", (void*)(this->sector_one));
    
    //while(true) { asm volatile("hlt"); }
    // read the root directory:
    
    uint64_t n_clusters = 0;
    void *data = this->get_cluster_chain( this->root_cluster, &n_clusters );
    this->root_dir_fat = new fat_directory( NULL, this->root_cluster );
    this->root_dir_vfs = new vfs_directory( NULL, this->root_dir_fat, NULL ); // possibly fill the "parent" area with a pointer to the mount point?
    // we'll do that when the VFS is more fleshed-out
    
    this->root_dir_fat->d_entry.start_cluster_hi = 0;
    this->root_dir_fat->d_entry.start_cluster_lo = 2;
    
    this->root_dir_vfs->attr.fstype = "fat32";
    this->root_dir_vfs->name = NULL;
    
    fat_direntry *cur = (fat_direntry*)data;
    fat_file *cur_file = new fat_file( NULL, cur );
    
    for(int i=0;i<n_clusters;i++) {
        for(int j=0;j<(this->sectors_per_cluster*16);j++) { // (this->sectors_per_cluster*512) / 32 = (this->sectors_per_cluster*16)
            // iterate 16 times for each sector in this cluster
            if( cur->shortname[0] == 0 ) { // no more directory entries
                kprintf("fat32: end of entry list at entry %u, cluster %u\n", j, i);
                goto __fat32_read_root_directory_loop_end;
            }
            if( cur->shortname[0] != 0xE5 ) {
                if( cur->attr == 0x0F ) { // long name entry
                    kprintf("fat32: found long name directory entry (ent num %u)\n", j, i);
                    fat_longname *ln = new fat_longname;
                    memcpy( (void*)ln, (void*)cur, sizeof(fat_longname) );
                    kprintf("fat32: %.10s %.12s %.4s\n", ln->name_lo, ln->name_med, ln->name_hi);
                    cur_file->name_entries.add_end( ln );
                } else {
                    kprintf("fat32: found 8.3 directory entry (ent num %u, cluster %u)\n", j, i);
                    cur_file->d_entry = *cur;
                    
                    vfs_node *node;
                    if( cur->attr & 0x10 ) {
                        node = new vfs_directory( this->root_dir_vfs, (void*)cur_file, fat32_construct_longname( cur_file->name_entries ) );
                        node->fs_info = kmalloc( sizeof(fat_directory) );
                        memcpy( node->fs_info, (void*)cur_file, sizeof(fat_directory) );
                    } else {
                        node = new vfs_file( this->root_dir_vfs, (void*)cur_file, fat32_construct_longname( cur_file->name_entries ) );
                        node->fs_info = kmalloc( sizeof(fat_file) );
                        vfs_file *fn = (vfs_file*)node;
                        fn->size = cur->fsize;
                        memcpy( node->fs_info, (void*)cur_file, sizeof(fat_file) );
                    }
                    node->attr.read_only = ( cur->attr & 1 );
                    node->attr.hidden    = ( cur->attr & 2 );
                    node->attr.fstype = "fat32";
                    
                    this->root_dir_vfs->files.add( node );
                    this->root_dir_fat->files.add(cur_file);
                    cur_file = new fat_file( (fat_file*)this->root_dir_fat, cur );
                }
            }
            cur++;
        }
    }
    __fat32_read_root_directory_loop_end:
    delete cur_file;
    kfree(data);
}

vfs_directory *fat32_fs::read_directory( vfs_directory *parent, fat_directory *ent ) {
    uint32_t cluster = ((ent->d_entry.start_cluster_hi)<<16) | (ent->d_entry.start_cluster_lo);
    
    uint64_t n_clusters = 0;
    void *data = this->get_cluster_chain( cluster, &n_clusters );
    fat_direntry *cur = (fat_direntry*)data;
    fat_file *cur_file = new fat_file( ((fat_file*)(parent->fs_info)), cur );
    vfs_directory *vfs = new vfs_directory( parent, (void*)ent, fat32_construct_longname( ent->name_entries ) );
    
    vfs->attr.fstype = "fat32";
    char* name = fat32_construct_longname( ent->name_entries );
    vfs->name = (char*)kmalloc(strlen(name));
    for( int i=0; i<strlen((char*)name); i++) {
        vfs->name[i] = name[i];
    }
    
    for(int i=0;i<n_clusters;i++) {
        for(int j=0;j<(this->sectors_per_cluster*16);j++) { // (this->sectors_per_cluster*512) / 32 = (this->sectors_per_cluster*16)
            // iterate 16 times for each sector in this cluster
            if( cur->shortname[0] == 0 ) { // no more directory entries
                kprintf("fat32: end of entry list at entry %u, cluster %u\n", j, i);
                goto __fat32_read_directory_loop_end;
            }
            if( cur->shortname[0] != 0xE5 ) {
                if( cur->attr == 0x0F ) { // long name entry
                    kprintf("fat32: found long name directory entry (ent num %u)\n", j, i);
                    fat_longname *ln = new fat_longname;
                    memcpy( (void*)ln, (void*)cur, sizeof(fat_longname) );
                    kprintf("fat32: %.10s %.12s %.4s\n", ln->name_lo, ln->name_med, ln->name_hi);
                    cur_file->name_entries.add_end( ln );
                } else {
                    kprintf("fat32: found 8.3 directory entry (ent num %u, cluster %u)\n", j, i);
                    cur_file->d_entry = *cur;
                    kprintf("fat32: %.8s\n", cur->shortname);
                    
                    vfs_node *node;
                    if( cur->attr & 0x10 ) {
                        node = new vfs_directory( vfs, (void*)cur_file, fat32_construct_longname( cur_file->name_entries ) );
                        node->fs_info = kmalloc( sizeof(fat_directory) );
                        memcpy( node->fs_info, (void*)cur_file, sizeof(fat_directory) );
                    } else {
                        node = new vfs_file( vfs, (void*)cur_file, fat32_construct_longname( cur_file->name_entries ) );
                        node->fs_info = kmalloc( sizeof(fat_file) );
                        vfs_file *fn = (vfs_file*)node;
                        fn->size = cur->fsize;
                        memcpy( node->fs_info, (void*)cur_file, sizeof(fat_file) );
                    }
                    node->attr.read_only = ( cur->attr & 1 );
                    node->attr.hidden    = ( cur->attr & 2 );
                    node->attr.fstype = "fat32";
                    
                    vfs->files.add( node );
                    ent->files.add(cur_file);
                    cur_file = new fat_file( ((fat_file*)(parent->fs_info)), cur );
                }
            }
            cur++;
        }
    }
    __fat32_read_directory_loop_end:
    delete cur_file;
    kfree(data);
    return vfs;
}

vfs_file* fat32_fs::create_file( unsigned char* name, vfs_directory *parent ) { // create a zero-length file
    fat_directory *dir = (fat_directory*)(parent->fs_info);
    uint32_t cluster = ((dir->d_entry.start_cluster_hi)<<16) | (dir->d_entry.start_cluster_lo);
    
    fat_direntry *ent = new fat_direntry;
    fat_file *info = new fat_file( dir, ent );
    
    kprintf("fat32: generating basis-name...\n");
    
    this->fat32_generate_basisname( name, info );
    vector<fat_longname*> *longnames = fat32_split_longname( name, ent->shortname, ent->ext );
    
    bool update_first_cluster = false;
    
    ent->attr = 0;
    ent->nt_reserved = 0;
    ent->ctime_tenths = 0;
    ent->ctime = 0;
    ent->cdate = 0;
    ent->adate = 0;
    ent->wtime = 0;
    ent->wdate = 0;
    ent->fsize = 0;
    ent->start_cluster_hi = 0;
    ent->start_cluster_lo = 0;
    
    if( cluster == 0 ) {
        kprintf("fat: allocating cluster...\n");
        uint32_t first_cluster = this->allocate_cluster();
        kprintf("fat: allocated cluster %u\n", first_cluster);
        //this->write_fat(first_cluster, 0x0FFFFFF8); // don't need to do this, allocate_cluster() does it for us
        cluster = first_cluster;
        
        fat_directory *pdir = (fat_directory*)dir->parent;
        uint64_t p_n_clusters = 0;
        
        kprintf("fat: updating parent directory entry...\n");
        void *dir_data = this->get_cluster_chain( pdir->cluster, &p_n_clusters );
        fat_direntry *cur = this->fat32_find_direntry( dir_data, p_n_clusters, dir );
        cur->fsize = 0;
        cur->start_cluster_hi = (cluster >> 16) & 0xFFFF;
        cur->start_cluster_lo = cluster & 0xFFFF;
        kprintf("fat: writing out parent directory data...\n");
        this->write_cluster_chain( pdir->cluster, dir_data, p_n_clusters * (this->sectors_per_cluster*16) * 512 );
        kfree( dir_data );
    } else {
        kprintf("fat: parent directory at cluster %u\n", cluster);
    }
    
    logger_flush_buffer();
    //system_halt;
    
    // now, find a suitable space in the target directory:
    uint64_t n_clusters = 0;
    void *buf = this->get_cluster_chain( cluster, &n_clusters );
    
    kprintf("fat: read in parent directory data (%llu clusters) at %#p.\n", n_clusters, buf);

    //logger_flush_buffer();
	//	system_halt;
    
    fat_direntry* cur = this->fat32_find_free_direntry( buf, n_clusters, longnames->count()+1 );

    kprintf("fat: split name into %u long name entries\n", longnames->count());
    kprintf("fat: found free space at %#p (offset %#tx)\n", (void*)cur, ((ptrdiff_t)cur) - ((ptrdiff_t)buf));

    for(int i=0;i<longnames->count();i++) {
        memcpy( (void*)cur, (void*)(longnames->get(i)), sizeof(fat_longname) );
        cur++;
    }
    memcpy( (void*)cur, (void*)ent, sizeof(fat_direntry) );
    kprintf("fat: writing out to cluster %u.\n", cluster);
    this->write_cluster_chain( cluster, buf, n_clusters * this->sectors_per_cluster * 512 );

    kfree(buf);
    
    vfs_file *ret = new vfs_file( parent, (void*)info, NULL);
    info->name_entries = *longnames;
    info->cluster = 0;
    
    ret->name = (char*)kmalloc(strlen((char*)name));
    for( int i=0; i<strlen((char*)name); i++) {
        ret->name[i] = name[i];
    }
    ret->name[strlen((char*)name)] = '\0';
    ret->attr.fstype = "fat32";
    ret->size = 0;
    
    return ret;
}

vfs_directory* fat32_fs::create_directory( unsigned char* name, vfs_directory *parent ) { // create an empty directory
    fat_directory *dir = (fat_directory*)(parent->fs_info);
    uint32_t cluster = ((dir->d_entry.start_cluster_hi)<<16) | (dir->d_entry.start_cluster_lo);
    
    fat_direntry *ent = new fat_direntry;
    fat_file *info = new fat_file( dir, ent );
    
    this->fat32_generate_basisname( name, info );
    vector<fat_longname*> *longnames = fat32_split_longname( name, ent->shortname, ent->ext );
    
    ent->attr = 0x10;
    ent->nt_reserved = 0;
    ent->ctime_tenths = 0;
    ent->ctime = 0;
    ent->cdate = 0;
    ent->adate = 0;
    ent->wtime = 0;
    ent->wdate = 0;
    ent->fsize = 0;
    ent->start_cluster_hi = 0;
    ent->start_cluster_lo = 0;
    
    if( cluster == 0 ) {
        uint32_t first_cluster = this->allocate_cluster();
        this->write_fat(first_cluster, 0x0FFFFFF8);
        cluster = first_cluster;
        
        fat_directory *pdir = (fat_directory*)dir->parent;
        uint64_t p_n_clusters = 0;
        
        void *dir_data = this->get_cluster_chain( pdir->cluster, &p_n_clusters );
        fat_direntry *cur = this->fat32_find_direntry( dir_data, n_clusters, dir );
        
        cur->fsize = 0;
        cur->start_cluster_hi = (cluster >> 16) & 0xFFFF;
        cur->start_cluster_lo = cluster & 0xFFFF;
        
        this->write_cluster_chain( pdir->cluster, dir_data, p_n_clusters * (this->sectors_per_cluster*16) * 512 );
    }
    
    // now, find a suitable space in the target directory:
    uint64_t n_clusters = 0;
    void *buf = this->get_cluster_chain( cluster, &n_clusters );

    fat_direntry *cur = this->fat32_find_free_direntry( buf, n_clusters, longnames->count()+1 );
    for(int i=0;i<longnames->count();i++) {
        memcpy( (void*)cur, (void*)(longnames->get(i)), sizeof(fat_longname) );
        cur++;
    }
    memcpy( (void*)cur, (void*)ent, sizeof(fat_direntry) );
    this->write_cluster_chain( cluster, buf, n_clusters * (this->sectors_per_cluster*16) * 512 );
    
    kfree(buf);
    
    vfs_directory *ret = new vfs_directory( parent, (void*)info, NULL );
    info->name_entries = *longnames;
    info->cluster = 0;
    
    ret->name = (char*)kmalloc(strlen((char*)name));
    for( int i=0; i<strlen((char*)name); i++) {
        ret->name[i] = name[i];
    }
    ret->name[strlen((char*)name)] = '\0';
    ret->attr.fstype = "fat32";
    
    return ret;
}

void fat32_fs::delete_file( vfs_file *file ) {
    fat_file *fn = (fat_file*)file->fs_info;
    fat_directory *dir = (fat_directory*)fn->parent;
    
    // step 1: free clusters
    uint64_t n_clusters = 0;
    vector<uint32_t> *clusters = this->read_cluster_chain( fn->cluster, &n_clusters );
    
    for( int i=0;i<n_clusters;i++) {
        this->write_fat( clusters->get(i), 0 );
    }
    
    // step 2: remove directory entries
    n_clusters = 0;
    void *dir_data = this->get_cluster_chain( dir->cluster, &n_clusters );
    fat_direntry *cur = this->fat32_find_direntry( dir_data, n_clusters, fn );
    fat_direntry *n = cur;
    int k = 0;
    do {
        n--;
        k++;
    } while( (n->attr == 0x0F) && (n >= dir_data) );
    // delete everything from n to cur, not including n itself but including cur.
    while(k > 0) {
        n++;
        fat_longname *ln = (fat_longname*)n;
        ln->seq_num = 0xE5;
        k--;
    }
    this->write_cluster_chain( dir->cluster, dir_data, n_clusters * (this->sectors_per_cluster*16) * 512 );
    kfree(dir_data);
    return;
}

void fat32_fs::read_file( vfs_file *file, void* out ) {
    fat_file *fn = (fat_file*)file->fs_info;
    
    if( fn->cluster == 0 ) // does the file exist?
        return;
    
    void *cluster_data = this->get_cluster_chain( fn->cluster, NULL );
    memcpy( out, cluster_data, file->size );
    kfree(cluster_data);
}

void fat32_fs::write_file( vfs_file *file, void* in, size_t size ) {
    fat_file *fn = (fat_file*)file->fs_info;
    uint32_t cluster = ((fn->d_entry.start_cluster_hi)<<16) | (fn->d_entry.start_cluster_lo);
    bool update_first_cluster = false;
    
    if( cluster == 0 ) {
        // step 0: add initial clusters
        uint32_t n_clusters_added = (size / 512) / (this->sectors_per_cluster);
        if( (size % (512 * this->sectors_per_cluster)) > 0 ) {
            n_clusters_added++;
        }
        
        uint32_t last_cluster = 0; 
        uint32_t first_cluster = 0;
        for(int i=0;i<n_clusters_added;i++) {
            uint32_t next_cluster = this->allocate_cluster();
            if( last_cluster > 0 ) {
                this->write_fat(last_cluster, next_cluster);
            } else {
                first_cluster = next_cluster;
            }
            last_cluster = next_cluster;
        }
        this->write_fat(last_cluster, 0x0FFFFFF8);
        cluster = first_cluster;
        update_first_cluster = true;
    }
    
    // step 1: modify clusters
    this->write_cluster_chain( cluster, in, size );
    
    // step 2: modify directory entries
    uint64_t n_clusters = 0;
    void *dir_data = this->get_cluster_chain( fn->parent->cluster, &n_clusters );
    fat_direntry *cur = this->fat32_find_direntry( dir_data, n_clusters, fn );
    cur->fsize = size;
    if( update_first_cluster ) {
        cur->start_cluster_hi = (cluster >> 16) & 0xFFFF;
        cur->start_cluster_lo = cluster & 0xFFFF;
    }
    this->write_cluster_chain( fn->parent->cluster, dir_data, n_clusters * (this->sectors_per_cluster*16) * 512 );
    kfree(dir_data);
    
    // step 3: modify VFS node
    fn->d_entry.fsize = size;
    if( update_first_cluster ) {
        fn->d_entry.start_cluster_hi = (cluster >> 16) & 0xFFFF;
        fn->d_entry.start_cluster_lo = cluster & 0xFFFF;
        fn->cluster = cluster;
    }
}

void fat32_fs::copy_file( vfs_file *file, vfs_directory *dst ) {
    fat_file *fn = (fat_file*)file->fs_info;
    fat_directory *src_dir = (fat_directory*)fn->parent;
    fat_directory *dst_dir = (fat_directory*)dst->fs_info;
    uint32_t cluster = ((fn->d_entry.start_cluster_hi)<<16) | (fn->d_entry.start_cluster_lo);
    
    // step 1: copy clusters
    uint32_t dest_cluster = this->allocate_cluster();
    uint64_t n_clusters = 0;
    void *data = this->get_cluster_chain( cluster, &n_clusters );
    this->write_cluster_chain( dest_cluster, data, fn->d_entry.fsize );
    kfree(data);
    
    // step 2: copy directory entry
    uint64_t n_dst_clusters = 0;
    uint64_t n_src_clusters = 0;
    void *src_cluster_data = this->get_cluster_chain( src_dir->cluster, &n_src_clusters );
    void *dest_cluster_data = this->get_cluster_chain( dst_dir->cluster, &n_dst_clusters );
    if( this->fat32_find_direntry( dest_cluster_data, n_clusters, fn ) ) {
        // destination file already exists
        return;
    } else {
        fat_direntry *cur = this->fat32_find_direntry( src_cluster_data, n_src_clusters, fn );
        fat_direntry *n = cur;
        int k = 0;
        do {
            n--;
            k++;
        } while( (n->attr == 0x0F) && (n >= src_cluster_data) );
        // copy everything from n to cur, not including n itself but including cur.
        fat_direntry *to = this->fat32_find_free_direntry( dest_cluster_data, n_dst_clusters, k );
        while(k > 0) { // copy longnames...
            n++;
            memcpy( (void*)to, (void*)n, sizeof(fat_direntry) );
            k--;
            to++;
        }
        this->write_cluster_chain( dst_dir->cluster, dest_cluster_data, n_dst_clusters * (this->sectors_per_cluster*16) * 512 );
    }
    
    kfree(src_cluster_data);
    kfree(dest_cluster_data);
}

void fat32_fs::move_file( vfs_file *file, vfs_directory *dst ) {
    this->copy_file( file, dst );
    this->delete_file( file );
}

fat_direntry *fat32_fs::fat32_find_direntry( void *cluster_data, uint64_t n_clusters, fat_file *fn ) {
    fat_direntry *cur = (fat_direntry*)cluster_data;
    for(int i=0;i<n_clusters;i++) {
        for(int j=0;j<(this->sectors_per_cluster*16);j++) {
            if( cur->shortname[0] == 0 )
                goto __fat32_find_direntry_loop_end;
                
            if( cur->attr != 0x0F ) {
                if( cur->shortname[0] != 0xE5 ) {
                    bool matched = true;
                    for(int k=0;k<8;k++ ) {
                        if( cur->shortname[k] != fn->d_entry.shortname[i] ) {
                            matched = false;
                            break;
                        }
                    }
                    if( matched ) {
                        for(int k=0;k<3;k++ ) {
                            if( cur->ext[k] != fn->d_entry.ext[i] ) {
                                matched = false;
                                break;
                            }
                        }
                    }
                    if( matched ) {
                        return cur;
                    }
                }
            }
            cur++;
        }
    }
    __fat32_find_direntry_loop_end:
    return NULL;
}

fat_direntry *fat32_fs::fat32_find_free_direntry( void *cluster_data, uint64_t n_clusters, unsigned int n_free ) {
    bool saw_e5 = false;
    uint32_t last_e5 = 0;
    uint32_t n_e5s = 0;
    
    fat_direntry *start = (fat_direntry*)cluster_data;
    fat_direntry *cur = (fat_direntry*)cluster_data;
    uint32_t target_e5 = 0; // start+target_e5 == the space we want to use
    int k = 0;
    for(int i=0;i<n_clusters;i++) {
        for(int j=0;j<(this->sectors_per_cluster*16);j++) { // iterate 16 times per sector, times this->sectors_per_cluster for each cluster
            if( cur->shortname[0] == 0xE5 ) {
                if( saw_e5 ) {
                    n_e5s++;
                    if( n_e5s == n_free ) {
                        target_e5 = last_e5;
                        goto __fat32_find_free_direntry_found_space;
                    }
                } else {
                    last_e5 = k;
                    saw_e5 = true;
                    n_e5s = 1;
                }
            } else if( cur->shortname[0] == 0 ) {
                if( saw_e5 ) {
                    target_e5 = last_e5;
                } else {
                    target_e5 = k;
                }
                goto __fat32_find_free_direntry_found_space;
            } else {
                saw_e5 = false;
            }
            cur++;
            k++;
        }
    }
    __fat32_find_free_direntry_found_space:
    return (start+target_e5);
}

uint64_t fat32_fs::cluster_to_lba(uint32_t cluster) {
    //return this->first_usable_cluster + cluster * this->sectors_per_cluster - (2*this->sectors_per_cluster);
    return ((cluster-2)*this->sectors_per_cluster) + this->first_usable_cluster;
}

void *fat32_fs::get_cluster( uint32_t cluster ) {
    uint64_t lba = this->cluster_to_lba( cluster );
    void *buf = kmalloc( this->sectors_per_cluster * 512 );
    
    io_read_partition( this->part_no, buf, lba*512, this->sectors_per_cluster * 512 );
    return buf;
}

void fat32_fs::write_cluster( uint32_t cluster, void* data ) {
    uint64_t lba = this->cluster_to_lba( cluster );
    
    io_write_partition( this->part_no, data, lba*512, this->sectors_per_cluster * 512 );
}

void *fat32_fs::get_clusters( vector<uint32_t> *clusters ) {
    void *buf = kmalloc( clusters->count() * this->sectors_per_cluster * 512 );
    void *out = buf;
    uintptr_t out_int = (uintptr_t)buf;
    for( int i=0;i<clusters->count();i++ ) {
        io_read_partition( this->part_no, buf, this->cluster_to_lba( clusters->get(i) )*512, this->sectors_per_cluster * 512 );
        out_int += (this->sectors_per_cluster * 512);
        buf = (void*)out_int;
    }
    return out;
}

vector<uint32_t> *fat32_fs::read_cluster_chain( uint32_t start, uint64_t* n_clusters ) {
    uint32_t current = start;
    uint32_t next = 0;
    uint32_t fat_sector = this->n_reserved_sectors + ( (current*4) / 512 );
    bool do_read = true;
    vector<uint32_t> *cluster_list = new vector<uint32_t>;
    
    kprintf("fat32: reading cluster chain starting from cluster %u.\n", start);
    
    void *buf = kmalloc(512);
    do {
        uint32_t fat_offset = (current*4) % 512;
        
        if( do_read ) {
            io_read_partition( this->part_no, buf, fat_sector*512, 512 );
            do_read = false;
        }
        
        uint8_t* cluster = (uint8_t*)buf;
        
        next = *((uint32_t*)(cluster+fat_offset)) & 0x0FFFFFFF;
        if( next != 0 ) // if this is actually allocated....
            cluster_list->add_end( current );
        
        kprintf("fat32: current cluster=%u / %#x\n", current, current);
        kprintf("fat32: data at %#p\n", buf);
        
        /*
        logger_flush_buffer();
        system_halt;
        */
        
        if( ( (next != 0) && !( (next & 0x0FFFFFFF) >= 0x0FFFFFF8 ) ) && ( (this->n_reserved_sectors + ( (current*4) / 512 )) != (this->n_reserved_sectors + ( (next*4) / 512 )) ) ) { // if this isn't the end of the chain and the next sector to read is different...
            do_read = true;
            fat_sector = (this->n_reserved_sectors + ( (next*4) / 512 ));
        }
        
        current = next;
        
        if( n_clusters != NULL )
            (*n_clusters)++;
        kprintf("fat32: next cluster=%u / %#x\n", next, next);
    } while( (next != 0) && !( (next & 0x0FFFFFFF) >= 0x0FFFFFF8 ) );
    kfree(buf);
    
    return cluster_list;
}

void fat32_fs::write_cluster_chain( uint32_t cluster, void* data, size_t len ) { // overwrite an existing file's data
    uint64_t n_clusters = 0;
    vector<uint32_t> *cluster_list = this->read_cluster_chain( cluster, &n_clusters );
    uint32_t new_clusters = (len / 512) / this->sectors_per_cluster;
    
    if( n_clusters > new_clusters ) { // shrink the file
        uint32_t diff = n_clusters - new_clusters;
        this->shrink_chain( cluster, diff );
    } else if( n_clusters < new_clusters ) { // extend the file
        uint32_t diff = new_clusters - n_clusters;
        this->extend_chain( cluster, diff );
    }  // file cluster chain length stays the same
    
    delete cluster_list;
    n_clusters = 0;
    
    cluster_list = this->read_cluster_chain( cluster, &n_clusters );
    void* cur = data;
    for( int i=0;i<(n_clusters-1);i++ ) {
        write_cluster( cluster_list->get(i), data );
        uintptr_t ptr_int = (uintptr_t)cur;
        ptr_int += (this->sectors_per_cluster * 512);
        cur = (void*)ptr_int;
    }
    
    void *last_cluster = kmalloc( this->sectors_per_cluster * 512 );
    size_t overflow = len % ( this->sectors_per_cluster * 512 );
    memcpy( last_cluster, cur, overflow );
    write_cluster( cluster_list->get(n_clusters-1), data );
    
    delete cluster_list;
    kfree(last_cluster);
}

void *fat32_fs::get_cluster_chain( uint32_t start, uint64_t* n_clusters ) {
    vector<uint32_t> *clusters = this->read_cluster_chain( start, n_clusters );
    
    void* ret = this->get_clusters( clusters );
    delete clusters;
    return ret;
}

unsigned int fat32_fs::allocate_cluster() {
    unsigned int current_fat_sector = this->n_reserved_sectors; // relative to this->n_reserved_sectors
    unsigned int current_cluster = 2;
    bool do_read = true;
    bool found = false;
    void *buf = kmalloc(512);
    uintptr_t ptr_int = (uintptr_t)buf;
    
    io_read_partition( this->part_no, buf, 512, 512 );
    
    uint32_t last_allocated = *((uint32_t*)(ptr_int+492));
    if( last_allocated != 0xFFFFFFFF ) {
        if( last_allocated < n_clusters ) {
            current_cluster = last_allocated+1;
            current_fat_sector = (this->n_reserved_sectors + ( (current_cluster*4) / 512 ));
        }
    }
    
    memclr(buf, 512);
    
    do {
        if( do_read ) {
            io_read_partition( this->part_no, buf, current_fat_sector*512, 512 );
            do_read = false;
        }
        
        uint32_t fat_offset = (current_cluster*4) % 512;
        uint8_t* cluster = (uint8_t*)buf;
        
        uint32_t cluster_entry = *((uint32_t*)(&cluster[fat_offset])) & 0x0FFFFFFF;
        if( cluster_entry == 0 ) {
            found = true;
            unsigned int hi_nibble = (*((uint32_t*)(&cluster[fat_offset])) & 0xF0000000);
            *((uint32_t*)(&cluster[fat_offset])) = hi_nibble | 0x0FFFFFF8;
            io_write_partition( this->part_no, buf, current_fat_sector * 512, 512 );
            
            io_read_partition( this->part_no, buf, 512, 512 );
    
            *((uint32_t*)(ptr_int+492)) = current_cluster;
            
            io_write_partition( this->part_no, buf, 512, 512 );
            
            break;
        }
        
        current_cluster++;
        if( current_fat_sector != (this->n_reserved_sectors + ( (current_cluster*4) / 512 )) ) {
            current_fat_sector = (this->n_reserved_sectors + ( (current_cluster*4) / 512 ));
            do_read = true;
        }
    } while( current_cluster < this->n_clusters );
    
    kfree(buf);
    
    if( found )
        return current_cluster;
    return 0;
}

void fat32_fs::write_fat( uint32_t cluster, uint32_t value ) {
    uint32_t fat_sector = this->n_reserved_sectors + ( (cluster*4) / 512 );
    uint32_t fat_offset = (cluster*4) % 512;
    
    void *buf = kmalloc(512);
    uint8_t* cluster_data = (uint8_t*)buf;
    uintptr_t ptr_int = (uintptr_t)buf;
    
    io_read_partition( this->part_no, buf, fat_sector*512, 512 );
    *((uint32_t*)(&cluster_data[fat_offset])) = (*((uint32_t*)(&cluster_data[fat_offset])) & 0xF0000000) | (value & 0x0FFFFFFF);
    io_write_partition( this->part_no, buf, fat_sector * 512, 512 );
    
    kfree(buf);
}

unsigned int fat32_fs::read_fat( uint32_t cluster ) {
    uint32_t fat_sector = this->n_reserved_sectors + ( (cluster*4) / 512 );
    uint32_t fat_offset = (cluster*4) % 512;
    
    void *buf = kmalloc(512);
    uint8_t* cluster_data = (uint8_t*)buf;
    uintptr_t ptr_int = (uintptr_t)buf;
    
    io_read_partition( this->part_no, buf, fat_sector*512, 512 );
    uint32_t ret = (*((uint32_t*)(&cluster_data[fat_offset])) & 0x0FFFFFFF);
    
    kfree(buf);
    
    return ret;
}

// extend a file by n_added_clusters clusters
void fat32_fs::extend_chain( uint32_t start_cluster, uint32_t n_added_clusters ) { 
    uint64_t current_n_clusters = 0;
    vector<uint32_t> *current_list = this->read_cluster_chain( start_cluster, &current_n_clusters );
    uint32_t last_cluster = current_list->get(current_n_clusters);
    for(int i=0;i<n_added_clusters;i++) {
        uint32_t next_cluster = this->allocate_cluster();
        this->write_fat(last_cluster, next_cluster);
        last_cluster = next_cluster;
    }
    this->write_fat(last_cluster, 0x0FFFFFF8);
    delete current_list;
}

// shrink a file by n_freed_clusters clusters
void fat32_fs::shrink_chain( uint32_t start_cluster, uint32_t n_freed_clusters ) {
    uint64_t current_n_clusters = 0;
    vector<uint32_t> *current_list = this->read_cluster_chain( start_cluster, &current_n_clusters );
    uint32_t last_cluster = current_list->get(current_n_clusters--);
    for(int i=0;i<n_freed_clusters;i++) {
        this->write_fat(last_cluster, 0);
        last_cluster = current_list->get(current_n_clusters--);
    }
    this->write_fat(last_cluster, 0x0FFFFFF8);
    delete current_list;
}

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

void fat32_fs::fat32_generate_basisname( unsigned char *name, fat_file *file ) {
    fat_directory *dir = (fat_directory*)file->parent;
    fat_direntry  *ent = &(file->d_entry);
    
    uint64_t n_clusters = 0;
    void* dir_data = this->get_cluster_chain( ((dir->d_entry.start_cluster_hi)<<16) | (dir->d_entry.start_cluster_lo), &n_clusters );
    
    int len = strlen((char*)name);
    int last_period_pos = 0;
    bool generate_numeric_tail = false;
    
    for( signed int i=strlen((char*)name);i>0;i--) {
        if( name[i-1] == '.' ) {
            last_period_pos = i-1;
            break;
        }
    }
    
    if( last_period_pos == 0 )
        last_period_pos = len;
        
    char* stripped_longname = (char*)kmalloc(last_period_pos+1);
    int stripped_period_pos = 0;
    int j = 0;
    
    for(int i=0;i<len;i++) {
        switch (name[i]) {
            case ' ':
                generate_numeric_tail = true;
                break;
            case '.':
                if( i == last_period_pos ) {
                    stripped_period_pos = j;
                    stripped_longname[j++] = name[i];
                } else {
                    generate_numeric_tail = true;
                }
                break;
            case ',':
            case '[':
            case ']':
            case ';':
            case '=':
            case '+':
                generate_numeric_tail = true;
                stripped_longname[j++] = '_';
                break;
            default:
                if( (name[i] <= 0x1F) || (name[i] >= 0x7E) ) {
                    generate_numeric_tail = true;
                    stripped_longname[j++] = '_';
                    break;
                } else if( (name[i] >= 0x61) && (name[i] <= 0x7A) ) { // lowercase to uppercase
                    stripped_longname[j++] = name[i]-0x20;
                } else {
                    stripped_longname[j++] = name[i];
                }
                break;
        }
    }
    
    //char* basisname_primary = kmalloc(8);
    
    if( stripped_period_pos < 8 ) {
        for( int i=0;i<stripped_period_pos;i++) {
            ent->shortname[i] = stripped_longname[i];
        }
        for( int i=stripped_period_pos;i<8;i++ ) {
            ent->shortname[i] = ' ';
        }
    } else {
        generate_numeric_tail = true;
        for( int i=0;i<8;i++) {
            ent->shortname[i] = stripped_longname[i];
        }
    }
    
    //char* basisname_extension = kmalloc(3);
    
    j = 0;
    for( int i=last_period_pos+1;((i<len) && (j<3));i++ ) {
        ent->ext[j++] = stripped_longname[i];
    }
    
    kfree(stripped_longname);
    
    // generate a numeric tail here
    int numeric_id = 1;
    char *test_tail = (char*)kmalloc(9);
    bool matches = false;
    
    do {
        // generate a test numeric tail string
        for( int i=0;i<8;i++) {
            test_tail[i] = ent->shortname[i];
        }
        char *num_id_str = itoa(numeric_id);
        test_tail[7-strlen(num_id_str)] = '~';
        for(int i=0;i<strlen(num_id_str);i++) {
            test_tail[7-i] = num_id_str[strlen(num_id_str)-(i+1)];
        }
        test_tail[8] = '\0';
        
        fat_direntry *cur = (fat_direntry*)dir_data;
        for(int i=0;i<n_clusters;i++) {
            for(int j=0;j<(this->sectors_per_cluster*16);j++) {
                if( cur->shortname[0] == 0 )
                    goto __fat32_generate_tail_loop_end;
                
                if( cur->attr != 0x0F ) { // if the current direntry is NOT a long file name....
                    bool sub_match = true;
                    for( int k=0;k<8;k++ ) {
                        if( cur->shortname[k] != test_tail[k] ) {
                            sub_match = false;
                            break;
                        }
                    }
                    if( sub_match ) {
                        matches = true;
                        goto __fat32_generate_tail_loop_end;
                    }
                }
                cur++;
            }
        }
        __fat32_generate_tail_loop_end:
        numeric_id++;
    } while( matches );
    
    for(int i=0;i<8;i++) {
        ent->shortname[i] = test_tail[i];
    }
    
    delete test_tail;
    kfree(dir_data);
}

uint8_t fat32_shortname_checksum( unsigned char* primary, unsigned char* ext ) {
    char shortname[11];
    for(int i=0;i<8;i++) {
        shortname[i] = primary[i];
    }
    for(int i=0;i<3;i++) {
        shortname[i+8] = ext[i];
    }
    
    uint8_t sum = 0;
    for( int i=11;i!=0;i-- ) {
        sum = ( (sum&1) ? 0x80 : 0 ) + (sum>>1) + shortname[11-i];
    }
    
    return sum;
}

vector<fat_longname*> *fat32_split_longname( unsigned char* name, unsigned char* shortname, unsigned char *ext ) {
    fat_longname *current = new fat_longname;
    vector<fat_longname*>* list = new vector<fat_longname*>;
    int len = strlen((char*)name);
    int n_emitted = 0;
    
    do {
        bool do_exit = false;
        for(int i=1;i<10;i+=2) {
            if( do_exit ) {
                current->name_lo[i-1] = 0xFF;
                current->name_lo[i] = 0xFF;
            } else if( n_emitted == len ) {
                current->name_lo[i-1] = 0;
                current->name_lo[i] = 0;
                do_exit = true;
            } else {
                current->name_lo[i] = name[n_emitted++];
            }
        }
        
        for(int i=1;i<12;i+=2) {
            if( do_exit ) {
                current->name_med[i-1] = 0xFF;
                current->name_med[i] = 0xFF;
            } else if( n_emitted == len ) {
                current->name_med[i-1] = 0;
                current->name_med[i] = 0;
                do_exit = true;
            } else {
                current->name_med[i] = name[n_emitted++];
            }
        }
        
        for(int i=1;i<4;i+=2) {
            if( do_exit ) {
                current->name_hi[i-1] = 0xFF;
                current->name_hi[i] = 0xFF;
            } else if( n_emitted == len ) {
                current->name_hi[i-1] = 0;
                current->name_hi[i] = 0;
                do_exit = true;
            } else {
                current->name_hi[i] = name[n_emitted++];
            }
        }
        current->seq_num = (list->count()+1) | (do_exit ? 0x40 : 0 );
        current->attr = 0x0F;
        current->type = 0;
        current->checksum = fat32_shortname_checksum( shortname, ext );
        list->add_end(current);
        current = new fat_longname;
    } while( n_emitted < len );
    delete current;
    
    return list;
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
    out[name_entries.count() * 13] = '\0';
    return out;
}

char *fat32_construct_shortname( fat_direntry* ent ) {
    int len = 0;
    for(int i=0;i<8;i++) {
        if( ent->shortname[i] == 0x20 ) {
            len = i;
            break;
        }
    }
    char *name = (char*)kmalloc(len+5);
    int j = 0;
    for(int i=0;i<len;i++) {
        name[j++] = ent->shortname[i];
    }
    name[j++] = '.';
    name[j++] = ent->ext[0];
    name[j++] = ent->ext[1];
    name[j++] = ent->ext[2];
    return name;
}
