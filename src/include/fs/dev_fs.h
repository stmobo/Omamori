/*
 * dev_fs.h
 *
 *  Created on: Jan 31, 2015
 *      Author: Tatantyler
 */

#pragma once
#include "includes.h"
#include "core/vfs.h"
#include "core/device_manager.h"

namespace device_manager {

	class dev_fs : public vfs_fs {
	public:
		vfs_file* create_file( unsigned char* name, vfs_directory* parent ) { return NULL; };
		vfs_directory* create_directory( unsigned char* name, vfs_directory* parent ) { return NULL; };
		void delete_file( vfs_file* file ) {};
		void read_file( vfs_file* file, void* buffer );
		void write_file( vfs_file* file, void* buffer, size_t size) {};
		vfs_file* copy_file( vfs_file* file, vfs_directory* destination ) { return NULL; };
		vfs_file* move_file( vfs_file* file, vfs_directory* destination ) { return NULL; };
		vfs_directory* read_directory( vfs_directory* parent, vfs_node *child );

		dev_fs();
	};

}
