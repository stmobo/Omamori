/*
 * dev.cpp
 *
 *  Created on: Jan 31, 2015
 *      Author: Tatantyler
 */

#include "includes.h"
#include "fs/dev_fs.h"

using namespace device_manager;

// the fs_info for a vfs_node in the devfs is a pointer to either the resource entry (for files)
// or the device node (for directories)

void dev_fs::read_file( vfs_file* file, void* buffer ) {
	device_resource* rsc = (device_resource*)file->fs_info;

	switch(rsc->type) {
	case res_type::memory:
	{
		char* data_out = (char*)kmalloc(18); // 8*2=16 chars for two 32-bit addresses, plus one for a newline and another for a terminator
		char* tmp = data_out;

		char* tmp2 = itoa( rsc->memory.start, 16 );
		strcpy( tmp, tmp2 );
		kfree(tmp2);

		tmp = (char*)(((uintptr_t)tmp)+8);
		*tmp = '\n';
		tmp++;

		tmp2 = itoa( rsc->memory.end, 16 );
		strcpy( tmp, tmp2 );
		kfree(tmp2);

		tmp = (char*)(((uintptr_t)tmp)+8);
		*tmp = '\0';

		memcpy( buffer, (void*)data_out, 18 );
		break;
	}
	case res_type::io_port:
	{
		char* data_out = (char*)kmalloc(10); // 4*2=8 chars for two 16-bit addresses, plus one for a newline and another for a terminator
		char* tmp = data_out;

		char* tmp2 = itoa( rsc->io_port.start, 16 );
		strcpy( tmp, tmp2 );
		kfree(tmp2);

		tmp = (char*)(((uintptr_t)tmp)+4);
		*tmp = '\n';
		tmp++;

		tmp2 = itoa( rsc->io_port.end, 16 );
		strcpy( tmp, tmp2 );
		kfree(tmp2);

		tmp = (char*)(((uintptr_t)tmp)+4);
		*tmp = '\0';

		memcpy( buffer, (void*)data_out, 10 );
		break;
	}
	case res_type::interrupt:
	{
		char* data_out = (char*)kmalloc(3); // 2 chars for 1 8-bit address, plus one for a terminator
		char* tmp = data_out;

		char* tmp2 = itoa( rsc->interrupt, 16 );
		strcpy( tmp, tmp2 );
		kfree(tmp2);

		tmp = (char*)(((uintptr_t)tmp)+2);
		*tmp = '\0';
		tmp++;

		memcpy( buffer, (void*)data_out, 3 );
		break;
	}
	default:
		break;
	}
}

void dev_fs::read_directory( vfs_directory* parent, vfs_directory *child ) {
	device_node* device = (device_node*)child->fs_info;

	//vfs_directory *out = new vfs_directory( parent, this, (void*)child->fs_info, child->name );
	for(unsigned int i=0;i<device->children.count();i++) {
		unsigned char* new_name = (unsigned char*)kmalloc(strlen(device->children[i]->human_name));

		strcpy( new_name, (unsigned char*)device->children[i]->human_name );

		for(unsigned int i=0;i<strlen(new_name);i++) {
			if( new_name[i] == ' ' ) {
				new_name[i] = '_';
			}
		}

		vfs_directory* dir = new vfs_directory( child, this, (void*)device->children[i], new_name  );
		child->files.add_end(dir);
	}

	//kprintf("dev_fs: device has %u resources.\n", device->resources.count());

	for(unsigned int i=0;i<device->resources.count();i++) {
		vfs_file* file = NULL;
		device_resource *rsc = device->resources[i];
		switch(rsc->type) {
		case res_type::memory:
		{
			char* n = itoa(i);
			file = new vfs_file( child, this, (void*)rsc, (unsigned char*)concatentate_strings( const_cast<char*>("memory-"), n ) );
			kfree(n);
			break;
		}
		case res_type::io_port:
		{
			char* n = itoa(i);
			file = new vfs_file( child, this, (void*)rsc, (unsigned char*)concatentate_strings( const_cast<char*>("ioport-"), n ) );
			kfree(n);
			break;
		}
		case res_type::interrupt:
		{
			char* n = itoa(i);
			file = new vfs_file( child, this, (void*)rsc, (unsigned char*)concatentate_strings( const_cast<char*>("interrupt-"), n ) );
			kfree(n);
			break;
		}
		default:
		case res_type::unknown:
		{
			continue;
		}
		}
		child->files.add_end(file);
	}

	//return out;
}

dev_fs::dev_fs() {
	device_node* device = (device_node*)&root;

	vfs_directory *out = new vfs_directory( NULL, this, (void*)device, NULL );
	for(unsigned int i=0;i<device->children.count();i++) {
		unsigned char* new_name = (unsigned char*)kmalloc(strlen(device->children[i]->human_name));

		strcpy( new_name, (unsigned char*)device->children[i]->human_name );

		for(unsigned int i=0;i<strlen(new_name);i++) {
			if( new_name[i] == ' ' ) {
				new_name[i] = '_';
			}
		}

		vfs_directory* dir = new vfs_directory( out, this, (void*)device->children[i], new_name  );
		out->files.add_end(dir);
	}

	for(unsigned int i=0;i<device->resources.count();i++) {
		vfs_file* file = NULL;
		switch(device->resources[i]->type) {
		case res_type::memory:
		{
			file = new vfs_file( out, this, (void*)device->resources[i], (unsigned char*)concatentate_strings( const_cast<char*>("memory-"), itoa(i) ) );
			break;
		}
		case res_type::io_port:
		{
			file = new vfs_file( out, this, (void*)device->resources[i], (unsigned char*)concatentate_strings( const_cast<char*>("ioport-"), itoa(i) ) );
			break;
		}
		case res_type::interrupt:
		{
			file = new vfs_file( out, this, (void*)device->resources[i], (unsigned char*)concatentate_strings( const_cast<char*>("interrupt-"), itoa(i) ) );
			break;
		}
		default:
		{
			file = NULL;
		}
		}
		if( file != NULL ) {
			out->files.add_end(file);
		}
	}

	this->base = out;

	this->base->expanded = true;
}
