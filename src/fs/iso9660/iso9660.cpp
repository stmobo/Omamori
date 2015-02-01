/*
 * iso9660.cpp
 *
 *  Created on: Jan 12, 2015
 *      Author: Tatantyler
 */

#include "fs/iso9660/iso9660.h"
#include "core/io.h"
#include "core/vfs.h"

void *iso9660::iso9660_fs::read_sector( uint32_t sector_num ) {
	void *sec_data = kmalloc(2048); // 1 sector = 2048 bytes / 2 KiB

	io_read_disk( this->device_id, sec_data, sector_num*2048, 2048 );

	return sec_data;
}

void *iso9660::iso9660_fs::read_direntry( iso9660_directory_entry* entry ) {
	void *data = kmalloc(entry->extent_size);

	void *sec_data = this->read_sector(entry->extent_address);
	memcpy(data, sec_data, entry->extent_size);

	kfree(sec_data);

	return data;
}

void iso9660::iso9660_fs::read_direntry( iso9660_directory_entry* entry, void *buf ) {
	void *data = kmalloc(entry->extent_size);

	void *sec_data = this->read_sector(entry->extent_address);
	memcpy(buf, sec_data, entry->extent_size);

	kfree(sec_data);
}

void iso9660::iso9660_fs::read_directory( vfs_directory* parent, vfs_directory *child ) {
	void *directory_data = this->read_direntry( (iso9660_directory_entry*)child->fs_info );
	//vfs_directory *out = new vfs_directory( parent, this, (void*)child->fs_info, child->name );

	iso9660_directory_entry* cur = (iso9660_directory_entry*)directory_data;

	//child->files.clear();

	while(cur->size > 0) {
		vfs_node *node;
		void *fs_data = kmalloc(cur->size);
		memcpy(fs_data, (void*)cur, cur->size);

		unsigned char *name = (unsigned char*)kmalloc(cur->file_ident_length);
		memcpy((void*)name, (void*)cur->file_identifier(), cur->file_ident_length);

		if(cur->flags & 0x02) {
			// directory
			vfs_directory *dir = new vfs_directory( (vfs_node*)child, this, (void*)fs_data, name );

			node = (vfs_node*)dir;
		} else {
			// file
			unsigned int actual_len = 0;
			for(unsigned int i=0;i<cur->file_ident_length;i++) {
				if( name[i] == ';' ) {
					break;
				}
				actual_len++;
			}
			unsigned char *truncated_name = (unsigned char*)kmalloc(actual_len);
			for(unsigned int i=0;i<actual_len;i++) {
				truncated_name[i] = name[i];
			}
			kfree(name);
			vfs_file *fn = new vfs_file( (vfs_node*)child, this, (void*)fs_data, truncated_name );
			fn->size = cur->extent_size;

			node = (vfs_node*)fn;
		}
		node->attr.read_only = true;
		child->files.add_end(node);
		cur = cur->next_directory();
	}

	kfree(directory_data);
	//return out;
}

void iso9660::iso9660_fs::read_file(vfs_file* file, void* buffer) {
	iso9660_directory_entry* entry = (iso9660_directory_entry*)file->fs_info;

	this->read_direntry(entry, buffer);
}

iso9660::iso9660_fs::iso9660_fs(unsigned int device_id) {
	this->device_id = device_id;
	// look for the primary volume descriptor
	void *pvd_data = NULL;
	unsigned int current_vd = 0x10;
	while(true) {
		void *vd_data = this->read_sector( current_vd );
		uint8_t type_code = ((uint8_t*)vd_data)[0];
		if(type_code == 1) {
			pvd_data = vd_data;
			kprintf("iso9660: found PVD at sector %u\n", current_vd);
			break;
		}
		if(type_code == 0xFF) {
			// could not find PVD
			kprintf("iso9660: could not find PVD\n");
			return;
		}
		kfree(vd_data);
		current_vd++;
	}

	if(pvd_data == NULL) {
		kprintf("iso9660: could not find PVD\n");
		return;
	}

	iso9660_directory_entry *root_entry = (iso9660_directory_entry*)kmalloc(34);
	memcpy( (void*)root_entry, (void*)(((uintptr_t)pvd_data)+156), 34 );

	unsigned char name[] = { '\\', '\0' };
	this->base = new vfs_directory( NULL, this, (void*)root_entry, name );

	void *directory_data = this->read_direntry(root_entry);
	iso9660_directory_entry *cur = (iso9660_directory_entry*)directory_data;
	while(cur->size > 0) {
		vfs_node *node = NULL;
		void *fs_data = kmalloc(cur->size);
		memcpy(fs_data, (void*)cur, cur->size);

		unsigned char *name = (unsigned char*)kmalloc(cur->file_ident_length);
		memcpy((void*)name, (void*)cur->file_identifier(), cur->file_ident_length);

		if(cur->flags & 0x02) {
			// directory
			if( strlen(name) > 0 ) {
				vfs_directory *dir = new vfs_directory( (vfs_node*)this->base, this, (void*)fs_data, name );

				node = (vfs_node*)dir;
				kprintf("iso9660: found directory: %s\n", name);
			}
		} else {
			// file
			unsigned int actual_len = 0;
			bool found_semicolon = false;
			for(unsigned int i=0;i<cur->file_ident_length;i++) {
				if( name[i] == ';' ) {
					found_semicolon = true;
					break;
				}
				actual_len++;
			}
			if( found_semicolon ) {
				unsigned char *truncated_name = (unsigned char*)kmalloc(actual_len);
				for(unsigned int i=0;i<actual_len;i++) {
					truncated_name[i] = name[i];
				}
				vfs_file *fn = new vfs_file( (vfs_node*)this->base, this, (void*)fs_data, truncated_name );
				fn->size = cur->extent_size;

				node = (vfs_node*)fn;
				kprintf("iso9660: found file (size %llu): %s\n", fn->size, truncated_name);
			}
			kfree(name);
		}
		if( node != NULL ) {
			node->attr.read_only = true;
			this->base->files.add_end(node);
		}
		cur = cur->next_directory();
	}

	kfree(directory_data);
	kfree(pvd_data);

	this->base->expanded = true;
}
