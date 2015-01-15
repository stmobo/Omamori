/*
 * iso9660.h
 *
 *  Created on: Jan 12, 2015
 *      Author: Tatantyler
 */

#pragma once
#include "core/io.h"
#include "core/vfs.h"

namespace iso9660 {

	struct iso9660_pvd_datetime {
		// all values are ASCII-coded decimal (except for tz)
		unsigned char year[4];
		unsigned char month[2];
		unsigned char day[2];
		unsigned char hour[2];
		unsigned char minute[2];
		unsigned char second[2];
		unsigned char centiseconds[2];
		uint8_t tz; // 0-100, where 0=GMT-12 and 100=GMT+13 (interval from -48 to 52)
	} __attribute__((packed));

	struct iso9660_dir_datetime {
		uint8_t year;
		uint8_t month;
		uint8_t day;
		uint8_t hour;
		uint8_t minute;
		uint8_t second;
		uint8_t tz;
	} __attribute__((packed));

	struct iso9660_directory_entry {
		uint8_t size;
		uint8_t ext_size;
		uint32_t extent_address;
		uint32_t extent_address_msb;
		uint32_t extent_size;
		uint32_t extent_size_msb;
		iso9660_dir_datetime record_datetime;
		uint8_t flags;
		uint8_t interleave_unit_size;
		uint8_t interleave_gap_size;
		uint16_t volume_sequence_no;
		uint16_t volume_sequence_no_msb;
		uint8_t file_ident_length;

		unsigned char* file_identifier() {
			return (unsigned char*)(((uintptr_t)this)+33);
		};

		iso9660_directory_entry* next_directory() {
			return (iso9660_directory_entry*)(((uintptr_t)this)+this->size);
		};
	} __attribute__((packed));

	class iso9660_fs : public vfs_fs {
		unsigned int device_id;
		void* read_sector( uint32_t sector_num );
		void* read_direntry( iso9660_directory_entry* entry );
		void read_direntry( iso9660_directory_entry* entry, void* buf );
	public:
		vfs_directory *base;

		vfs_file* create_file( unsigned char* name, vfs_directory* parent ) { return NULL; };
		vfs_directory* create_directory( unsigned char* name, vfs_directory* parent ) { return NULL; };
		void delete_file( vfs_file* file ) {};
		void read_file( vfs_file* file, void* buffer );
		void write_file( vfs_file* file, void* buffer, size_t size) {};
		void copy_file( vfs_file* file, vfs_directory* destination ) {};
		void move_file( vfs_file* file, vfs_directory* destination ) {};
		vfs_directory* read_directory( vfs_directory* parent, vfs_node *child );

		iso9660_fs( unsigned int device_id );
	};
};
