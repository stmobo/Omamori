/*
 * fat_fs.cpp
 *
 *  Created on: Jan 11, 2015
 *      Author: Tatantyler
 */

#include "fs/fat/fat_fs.h"
#include "core/vfs.h"
#include "core/io.h"

void fat_fs::fat_fs::read_directory( vfs_directory* parent, vfs_directory *out ) {
	fat_directory_entry *entry = (fat_directory_entry*)out->fs_info;
	//vfs_directory *out = new vfs_directory( parent, this, (void*)entry, child->name );

	//out->files.clear();

	fat_cluster_chain directory_chain(this, entry->start_cluster());
	void *directory_data = directory_chain.read();

	vector<unsigned char> name_buf;
	fat_directory_entry *cur = (fat_directory_entry*)directory_data;
	unsigned int bytes_read = 0;

	do {
		if(cur->shortname[0] == 0x00) {
			// no more directory entries
			break;
		}
		if(cur->shortname[0] == 0xE5) {
			// entry nonexistent, skip
			cur++;
			bytes_read += sizeof(fat_directory_entry);
			name_buf.clear();
			continue;
		}

		if(cur->attr == 0x0F) {
			// long file name
			fat_longname *lfn = (fat_longname*)cur;

			// read into buffer
			for(unsigned int i=0;i<10;i+=2) {
				if( (lfn->name_lo[i] == 0) || (lfn->name_lo[i] == 0xFF) )
					break;
				name_buf.add_end(lfn->name_lo[i]);
			}

			for(unsigned int i=0;i<12;i+=2) {
				if( (lfn->name_med[i] == 0) || (lfn->name_med[i] == 0xFF) )
					break;
				name_buf.add_end(lfn->name_med[i]);
			}

			for(unsigned int i=0;i<4;i+=2) {
				if( (lfn->name_hi[i] == 0) || (lfn->name_hi[i] == 0xFF) )
					break;
				name_buf.add_end(lfn->name_hi[i]);
			}
		} else {
			// parse 8.3 directory entry
			//fat_directory_entry ent = *cur;
			// copy to heap
			void *tmp = kmalloc(sizeof(fat_directory_entry));
			memcpy(tmp, cur, sizeof(fat_directory_entry));

			unsigned char *name;

			if(name_buf.count() > 0) {
				name = (unsigned char*)kmalloc(name_buf.count()+1);
				for(unsigned int i = 0; i < name_buf.count(); i++) {
					name[i] = name_buf[i];
				}
				name[name_buf.count()] = '\0';
			} else {
				name = ((fat_directory_entry*)tmp)->shortname;
			}

			vfs_node *node;
			if(cur->attr & FAT_FILE_ATTR_DIRECTORY) {
				node = new vfs_directory( out, this, tmp, name );
			} else {
				vfs_file *f = new vfs_file( out, this, tmp, name );
				f->size = cur->fsize;
				node = (vfs_node*)f;
			}

			out->files.add_end(node);
		}

		cur++;
		bytes_read += sizeof(fat_directory_entry);
	} while(bytes_read < directory_chain.get_size());

	kfree(directory_data);

	//return out;
}

// use to update fs_info in vfs_node
fat_fs::fat_directory_entry* fat_fs::fat_fs::read_dir_entry( vfs_directory *dir, unsigned char *shortname ) {
	fat_directory_entry *parent_entry = (fat_directory_entry*)dir->fs_info;

	fat_cluster_chain parent_chain(this, parent_entry->start_cluster());
	void *parent_data = parent_chain.read();
	fat_directory_entry *parent_dir_data = (fat_directory_entry*)parent_data;
	fat_directory_entry *end = (fat_directory_entry*)((uintptr_t)parent_dir_data + (parent_chain.clusters.count()*this->params.sectors_per_cluster*512));

	fat_directory_entry *ret = (fat_directory_entry*)kmalloc(sizeof(fat_directory_entry));

	bool success = false;
	while( (parent_dir_data < end) && (!success)  ) {
		bool matched = true;
		if( parent_dir_data->shortname[0] == 0 ) {
			return ret;
		}

		if( parent_dir_data->shortname[0] == 0xE5 ) {
			parent_dir_data++;
			continue;
		}

		for(unsigned int i=0;i<8;i++) {
			if( parent_dir_data->shortname[i] != shortname[i] ) {
				matched = false;
				break;
			}
		}

		if(matched) {
			memcpy( ret, parent_dir_data, sizeof(fat_directory_entry) );
			success = true;
			break;
		}

		parent_dir_data++;
	}

	kfree(parent_data);

	return ret;
}

// update one dir entry under <dir> with another
// returns true if the named directory entry was found, false otherwise
bool fat_fs::fat_fs::update_dir_entry( vfs_directory *dir, unsigned char *shortname, fat_directory_entry *rep ) {
	fat_directory_entry *parent_entry = (fat_directory_entry*)dir->fs_info;

	fat_cluster_chain parent_chain(this, parent_entry->start_cluster());
	void *parent_data = parent_chain.read();
	fat_directory_entry *parent_dir_data = (fat_directory_entry*)parent_data;
	fat_directory_entry *end = (fat_directory_entry*)((uintptr_t)parent_dir_data + (parent_chain.clusters.count()*this->params.sectors_per_cluster*512));

	bool success = false;
	while( (parent_dir_data < end) && (!success) ) {
		bool matched = true;
		if( parent_dir_data->shortname[0] == 0 ) {
			return false;
		}

		if( parent_dir_data->shortname[0] == 0xE5 ) {
			parent_dir_data++;
			continue;
		}

		for(unsigned int i=0;i<8;i++) {
			if( parent_dir_data->shortname[i] != shortname[i] ) {
				matched = false;
				break;
			}
		}

		if(matched) {
			memcpy(parent_dir_data, rep, sizeof(fat_directory_entry));
			success = true;
			break;
		}

		parent_dir_data++;
	}

	parent_chain.write( parent_data, parent_chain.clusters.count()*this->params.sectors_per_cluster*512 );
	kfree(parent_data);

	return success;
}

void fat_fs::fat_fs::update_node(vfs_node* node) {
	unsigned char shortname[8];
	fat_directory_entry* old_fsinfo = (fat_directory_entry*)node->fs_info;
	memcpy( (void*)shortname, (void*)old_fsinfo->shortname, 8 );

	kfree(old_fsinfo);
	node->fs_info = (void*)read_dir_entry( (vfs_directory*)node->parent, shortname );
	if(node->type == vfs_node_types::file) {
		vfs_file* fn = (vfs_file*)node;
		fn->size = ((fat_directory_entry*)node->fs_info)->fsize;
	}
}

vfs_file* fat_fs::fat_fs::create_file( unsigned char* name, vfs_directory* parent ) {
	fat_directory_entry *new_ent = new fat_directory_entry;

	new_ent->attr = 0;
	new_ent->nt_reserved = 0;
	new_ent->ctime_tenths = 0;
	new_ent->ctime = 0;
	new_ent->cdate = 0;
	new_ent->adate = 0;
	new_ent->wtime = 0;
	new_ent->wdate = 0;
	new_ent->fsize = 0;
	new_ent->start_cluster_hi = 0;
	new_ent->start_cluster_lo = 0;

	vfs_file *ret = new vfs_file( parent, this, (void*)new_ent, name );

	unsigned char *tmp = generate_basisname( ret );
	for(unsigned int i=0; i<8;i++) {
		new_ent->shortname[i] = tmp[i];
	}
	kfree(tmp);

	// TODO: write in the extension

	fat_directory_entry *parent_ent = (fat_directory_entry*)parent->fs_info;
	fat_cluster_chain *parent_chain;
	if( parent_ent->start_cluster() == 0 ) {
		uint32_t first_cluster = this->allocate_cluster();
		parent_chain = new fat_cluster_chain(this, first_cluster);
		parent_ent->start_cluster_hi = (uint16_t)((first_cluster >> 16) & 0xFFFF);
		parent_ent->start_cluster_lo = (uint16_t)(first_cluster & 0xFFFF);

		kprintf("fat_fs::create_file(): extending grandparent list\n");

		this->update_dir_entry( (vfs_directory*)parent->parent, parent_ent->shortname, parent_ent );
		/*
		parent_chain = new fat_cluster_chain;

		fat_cluster_chain grandparent_chain( ((fat_directory_entry*)(parent->parent->fs_info))->start_cluster() );
		void *grandparent_data = grandparent_chain.read();
		fat_directory_entry *grandparent_dir_data = (fat_directory_entry*)grandparent_data;
		fat_directory_entry *end = (fat_directory_entry*)((uintptr_t)grandparent_dir_data + (grandparent_chain.clusters.count()*this->params.sectors_per_cluster*512));

		while( grandparent_dir_data < end ) {
			bool matched = true;
			uint8_t *a = (uint8_t*)grandparent_dir_data;
			uint8_t *b = (uint8_t*)parent_ent;
			for(unsigned int i=0;i<sizeof(fat_directory_entry);i++) {
				if( a[i] != b[i] ) {
					matched = false;
					break;
				}
			}
			if(matched) {
				new_parent_ent->start_cluster_hi = (uint16_t)((first_cluster >> 16) & 0xFFFF);
				new_parent_ent->start_cluster_lo = (uint16_t)(first_cluster & 0xFFFF);
				break;
			}
			grandparent_dir_data++;
		}

		grandparent_chain.write(grandparent_data, (grandparent_chain.clusters.count()*this->params.sectors_per_cluster*512));
		kfree(grandparent_data);
		*/
	} else {
		parent_chain = new fat_cluster_chain(this, parent_ent->start_cluster());
		kprintf("fat_fs::create_file(): parent clusters start at %u.\n", parent_ent->start_cluster());
	}

	fat_directory_entry *parent_data = (fat_directory_entry*)parent_chain->read();
	void* parent_data_base = (void*)parent_data;

	unsigned int end_of_list = 0;
	while( parent_data->shortname[0] != 0 ) {
		parent_data++;
		end_of_list++;
	}

	kprintf("fat_fs::create_file(): found end of list at position %u.\n", end_of_list);

	// TODO: Handle extending the directory cluster chain

	vector<fat_longname> *lfns = generate_lfn_entries(name);
	for(unsigned int i=0; i<lfns->count();i++) {
		fat_longname lfn = lfns->get(i);
		memcpy((void*)parent_data, (void*)&lfn, sizeof(fat_longname));
		parent_data++;
	}
	memcpy((void*)parent_data, (void*)new_ent, sizeof(fat_directory_entry));

	parent_chain->write( parent_data_base, parent_chain->clusters.count()*this->params.sectors_per_cluster*512 );

	kfree(parent_data_base);
	delete parent_chain;

	return ret;
}

vfs_directory* fat_fs::fat_fs::create_directory( unsigned char* name, vfs_directory* parent ) {
	vfs_file *new_file = this->create_file(name, parent);

	vfs_directory *new_directory = new vfs_directory( parent, this, new_file->fs_info, name );
	fat_directory_entry *child_entry = (fat_directory_entry*)new_directory->fs_info;
	child_entry->attr |= FAT_FILE_ATTR_DIRECTORY;

	this->update_dir_entry( parent, child_entry->shortname, child_entry );

	/*
	fat_cluster_chain parent_chain(parent_entry->start_cluster());
	void *parent_data = parent_chain.read();
	fat_directory_entry *parent_dir_data = (fat_directory_entry*)parent_data;
	fat_directory_entry *end = (fat_directory_entry*)((uintptr_t)parent_dir_data + (parent_chain.clusters.count()*this->params.sectors_per_cluster*512));

	while( parent_dir_data < end ) {
		bool matched = true;
		uint8_t *a = (uint8_t*)parent_dir_data;
		uint8_t *b = (uint8_t*)child_entry;
		for(unsigned int i=0;i<sizeof(fat_directory_entry);i++) {
			if( a[i] != b[i] ) {
				matched = false;
				break;
			}
		}

		if(matched) {
			parent_dir_data->attr |= FAT_FILE_ATTR_DIRECTORY;
			child_entry->attr |= FAT_FILE_ATTR_DIRECTORY;
			break;
		}

		parent_dir_data++;
	}

	parent_chain.write( parent_data, parent_chain.clusters.count()*this->params.sectors_per_cluster*512 );
	kfree(parent_data);
	*/

	kfree(new_file);

	return new_directory;
}

void fat_fs::fat_fs::delete_file( vfs_file* file ) {
	fat_directory_entry *child_entry = (fat_directory_entry*)file->fs_info;
	fat_directory_entry *old_child_entry = (fat_directory_entry*)kmalloc(sizeof(fat_directory_entry));
	memcpy( (void*)old_child_entry, (void*)child_entry, sizeof(fat_directory_entry) );

	child_entry->shortname[0] = 0xE5;
	child_entry->start_cluster_lo = 0;
	child_entry->start_cluster_hi = 0;

	this->update_dir_entry( (vfs_directory*)file->parent, old_child_entry->shortname, child_entry );

	kfree(old_child_entry);
}

void fat_fs::fat_fs::read_file( vfs_file* file, void* buffer ) {
	this->update_node(file);

	fat_directory_entry *child_entry = (fat_directory_entry*)file->fs_info;

	if(child_entry->start_cluster() != 0) {
		fat_cluster_chain child_chain(this, child_entry->start_cluster());
		void *data = child_chain.read();

		if( data == NULL ) {
			kprintf("Could not read in child chain?\n");
			logger_flush_buffer();
			system_halt;
		}

		//kprintf("fat: data at %#p\n", data);
		//kprintf("fat: reading in %u / %u bytes from %#p to %#p\n", child_entry->fsize, file->size, data, buffer);

		//logger_flush_buffer();
		//system_halt;

		memcpy(buffer, data, file->size);

		kfree(data);
	}
}

void fat_fs::fat_fs::write_file( vfs_file* file, void* buffer, size_t size ) {
	fat_directory_entry *child_entry = (fat_directory_entry*)file->fs_info;

	child_entry->fsize = size;
	file->size = size;
	// TODO: update times here too

	if(child_entry->start_cluster() != 0) {
		kprintf("fat: cluster chain for %s starts at cluster %u (%#x)\n", file->name, child_entry->start_cluster(), child_entry->start_cluster());
		fat_cluster_chain child_chain(this, child_entry->start_cluster());

		child_chain.write(buffer, size);
	} else {
		uint32_t first_cluster = this->allocate_cluster();
		kprintf("fat: allocated new cluster chain for %s at cluster %u (%#x)\n", file->name, first_cluster, first_cluster);
		fat_cluster_chain child_chain(this, first_cluster);

		child_entry->start_cluster_hi = (uint16_t)((first_cluster >> 16) & 0xFFFF);
		child_entry->start_cluster_lo = (uint16_t)(first_cluster & 0xFFFF);

		child_chain.write(buffer, size);
		this->write_fat(first_cluster, 0x0FFFFFF8);
	}

	this->update_dir_entry( (vfs_directory*)file->parent, child_entry->shortname, child_entry );

	this->update_node(file);
}

vfs_file* fat_fs::fat_fs::copy_file( vfs_file* file, vfs_directory* destination ) {
	fat_directory_entry *src_entry = (fat_directory_entry*)file->fs_info;
	uint32_t dest_start_cluster = 0;

	if( src_entry->start_cluster() != 0 ) {
		// allocate new cluster chain,
		// copy cluster data

		uint32_t new_cluster = this->allocate_cluster();

		fat_cluster_chain dest_cluster(this, new_cluster);
		fat_cluster_chain src_cluster(this, src_entry->start_cluster());

		void *tmp = src_cluster.read();
		dest_cluster.write(tmp, src_cluster.clusters.count()*this->params.sectors_per_cluster*512);
		dest_start_cluster = new_cluster;
	}

	// generate / copy metadata
	fat_directory_entry *new_ent = new fat_directory_entry;

	*new_ent = *src_entry;
	new_ent->start_cluster_hi = (dest_start_cluster>>16) & 0xFFFF;
	new_ent->start_cluster_lo = dest_start_cluster & 0xFFFF;

	unsigned char *name_copy = (unsigned char*)kmalloc(strlen((char*)file->name)+1);
	strcpy( (char*)name_copy, (char*)file->name, 0 );

	vfs_file *ret = new vfs_file( destination, this, &new_ent, name_copy );

	unsigned char *tmp = generate_basisname( ret );
	for(unsigned int i=0; i<8;i++) {
		new_ent->shortname[i] = tmp[i];
	}
	kfree(tmp);

	// and now to actually write it out
	fat_cluster_chain dest_parent_cluster_chain( this, ((fat_directory_entry*)destination->fs_info)->start_cluster() );
	void *dest_parent_data_base = dest_parent_cluster_chain.read();
	fat_directory_entry* dest_parent_data = (fat_directory_entry*)dest_parent_data_base;

	vector<fat_longname> *lfns = generate_lfn_entries(name_copy);
	for(unsigned int i=0; i<lfns->count();i++) {
		fat_longname lfn = lfns->get(i);
		memcpy((void*)dest_parent_data, (void*)&lfn, sizeof(fat_longname));
		dest_parent_data++;
	}

	memcpy((void*)dest_parent_data, (void*)new_ent, sizeof(fat_directory_entry));

	dest_parent_cluster_chain.write( dest_parent_data_base, dest_parent_cluster_chain.clusters.count()*this->params.sectors_per_cluster*512 );

	kfree(dest_parent_data_base);

	return ret;
}

vfs_file* fat_fs::fat_fs::move_file( vfs_file* file, vfs_directory* destination ) {
	fat_directory_entry *src_entry = (fat_directory_entry*)file->fs_info;

	// generate / copy metadata
	fat_directory_entry *new_ent = new fat_directory_entry;

	*new_ent = *src_entry;
	src_entry->shortname[0] = 0;
	src_entry->start_cluster_hi = 0;
	src_entry->start_cluster_lo = 0;

	// copy this now before we overwrite it
	unsigned char shortname_copy[8];
	strcpy( (char*)shortname_copy, (char*)new_ent->shortname, 8 );

	unsigned char *name_copy = (unsigned char*)kmalloc(strlen((char*)file->name)+1);
	strcpy( (char*)name_copy, (char*)file->name, 0 );

	vfs_file *ret = new vfs_file( destination, this, &new_ent, name_copy );

	unsigned char *tmp = generate_basisname( ret );
	for(unsigned int i=0; i<8;i++) {
		new_ent->shortname[i] = tmp[i];
	}
	kfree(tmp);

	// and now to actually write it out
	fat_cluster_chain dest_parent_cluster_chain( this, ((fat_directory_entry*)destination->fs_info)->start_cluster() );
	void *dest_parent_data_base = dest_parent_cluster_chain.read();
	fat_directory_entry* dest_parent_data = (fat_directory_entry*)dest_parent_data_base;

	vector<fat_longname> *lfns = generate_lfn_entries(name_copy);
	for(unsigned int i=0; i<lfns->count();i++) {
		fat_longname lfn = lfns->get(i);
		memcpy((void*)dest_parent_data, (void*)&lfn, sizeof(fat_longname));
		dest_parent_data++;
	}

	memcpy((void*)dest_parent_data, (void*)new_ent, sizeof(fat_directory_entry));

	dest_parent_cluster_chain.write( dest_parent_data_base, dest_parent_cluster_chain.clusters.count()*this->params.sectors_per_cluster*512 );

	kfree(dest_parent_data_base);

	// delete the old direntry
	this->update_dir_entry( (vfs_directory*)file->parent, shortname_copy, src_entry );

	return ret;
}

fat_fs::fat_fs::fat_fs( unsigned int part_no ) {
	io_read_partition( part_no, (void*)(this->params.sector_one), 0, 512 );

	this->params.part_no = part_no;
	//this->bytes_per_sector     = *( (uint16_t*)( (uintptr_t)(sector_one+11) ) );
	this->params.sectors_per_cluster  = *( (uint8_t*) ( (uintptr_t)(this->params.sector_one+13) ) );
	this->params.n_reserved_sectors   = *( (uint16_t*)( (uintptr_t)(this->params.sector_one+14) ) );
	this->params.n_fats               = *( (uint16_t*)( (uintptr_t)(this->params.sector_one+16) ) );
	this->params.n_directory_entries  = *( (uint16_t*)( (uintptr_t)(this->params.sector_one+17) ) );
	if( *( (uint16_t*)( (uintptr_t)(this->params.sector_one+19) ) ) == 0 ) {
		this->params.n_sectors        = *( (uint32_t*)( (uintptr_t)(this->params.sector_one+32) ) );
	} else {
		this->params.n_sectors        = *( (uint16_t*)( (uintptr_t)(this->params.sector_one+19) ) );
	}
	this->params.n_hidden_sectors     = *( (uint32_t*)( (uintptr_t)(this->params.sector_one+28) ) );
	this->params.fat_size_sectors     = *( (uint32_t*)( (uintptr_t)(this->params.sector_one+36) ) );
	this->params.root_cluster         = *( (uint32_t*)( (uintptr_t)(this->params.sector_one+44) ) );
	this->params.fsinfo               = *( (uint16_t*)( (uintptr_t)(this->params.sector_one+48) ) );
	this->params.backup_boot_sector   = *( (uint16_t*)( (uintptr_t)(this->params.sector_one+50) ) );

	this->params.first_usable_cluster = this->params.n_reserved_sectors + (this->params.n_fats * this->params.fat_size_sectors);
	this->params.n_data_sectors = this->params.n_sectors - ( this->params.n_reserved_sectors + (this->params.n_fats * this->params.fat_size_sectors) );
	this->params.n_clusters = this->params.n_data_sectors / this->params.sectors_per_cluster;

	kprintf("fat32: reading partition %u as FAT32\n", part_no);
	kprintf("fat32: %u sectors per cluster\n", this->params.sectors_per_cluster);
	kprintf("fat32: %u FATs\n", this->params.n_fats);
	kprintf("fat32: %u directory entries\n", this->params.n_directory_entries);
	kprintf("fat32: %u hidden sectors\n", this->params.n_hidden_sectors);
	kprintf("fat32: %u reserved sectors\n", this->params.n_reserved_sectors);
	kprintf("fat32: %u sectors per FAT\n", this->params.fat_size_sectors);
	kprintf("fat32: root cluster located at cluster %u\n", this->params.root_cluster);
	kprintf("fat32: data located at %#p\n", (void*)(this->params.sector_one));

	fat_cluster_chain directory_chain(this, this->params.root_cluster);
	void *directory_data = directory_chain.read();

	fat_directory_entry *new_ent = new fat_directory_entry;

	unsigned char name[] = { '\\', '\0' };
	this->base = new vfs_directory(NULL, this, new_ent, name);

	new_ent->attr = FAT_FILE_ATTR_DIRECTORY;
	new_ent->nt_reserved = 0;
	new_ent->ctime_tenths = 0;
	new_ent->ctime = 0;
	new_ent->cdate = 0;
	new_ent->adate = 0;
	new_ent->wtime = 0;
	new_ent->wdate = 0;
	new_ent->fsize = 0;
	new_ent->start_cluster_hi = (this->params.root_cluster >> 16) & 0xFFFF;
	new_ent->start_cluster_lo = this->params.root_cluster & 0xFFFF;

	vector<unsigned char> name_buf;
	fat_directory_entry *cur = (fat_directory_entry*)directory_data;
	unsigned int bytes_read = 0;

	do {
		if(cur->shortname[0] == 0x00) {
			// no more directory entries
			break;
		}
		if(cur->shortname[0] == 0xE5) {
			// entry nonexistent, skip
			cur++;
			bytes_read += sizeof(fat_directory_entry);
			name_buf.clear();
			continue;
		}

		if(cur->attr == 0x0F) {
			// long file name
			fat_longname *lfn = (fat_longname*)cur;

			// read into buffer
			for(unsigned int i=0;i<10;i+=2) {
				if( (lfn->name_lo[i] == 0) || (lfn->name_lo[i] == 0xFF) )
					break;
				name_buf.add_end(lfn->name_lo[i]);
			}

			for(unsigned int i=0;i<12;i+=2) {
				if( (lfn->name_med[i] == 0) || (lfn->name_med[i] == 0xFF) )
					break;
				name_buf.add_end(lfn->name_med[i]);
			}

			for(unsigned int i=0;i<4;i+=2) {
				if( (lfn->name_hi[i] == 0) || (lfn->name_hi[i] == 0xFF) )
					break;
				name_buf.add_end(lfn->name_hi[i]);
			}
		} else {
			// parse 8.3 directory entry
			//fat_directory_entry ent = *cur;
			// copy to heap
			void *tmp = kmalloc(sizeof(fat_directory_entry));
			memcpy(tmp, cur, sizeof(fat_directory_entry));

			unsigned char *name;

			if(name_buf.count() > 0) {
				name = (unsigned char*)kmalloc(name_buf.count()+1);
				for(unsigned int i = 0; i < name_buf.count(); i++) {
					name[i] = name_buf[i];
				}
				name[name_buf.count()] = '\0';
				name_buf.clear();
			} else {
				name = ((fat_directory_entry*)tmp)->shortname;
			}

			vfs_node *node;
			if(cur->attr & FAT_FILE_ATTR_DIRECTORY) {
				node = new vfs_directory( this->base, this, tmp, name );
			} else {
				vfs_file *f = new vfs_file( this->base, this, tmp, name );
				f->size = cur->fsize;
				node = (vfs_node*)f;
			}

			this->base->files.add_end(node);
		}

		cur++;
		bytes_read += sizeof(fat_directory_entry);
	} while(bytes_read < directory_chain.get_size());

	this->base->expanded = true;

	kfree(directory_data);
}
