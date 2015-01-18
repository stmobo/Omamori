/*
 * fat_fs.h
 *
 *  Created on: Jan 11, 2015
 *      Author: Tatantyler
 */

#pragma once
#include "includes.h"
#include "core/vfs.h"

#define FAT_FILE_ATTR_RO        0x01
#define FAT_FILE_ATTR_HIDDEN    0x02
#define FAT_FILE_ATTR_SYSTEM    0x04
#define FAT_FILE_ATTR_VOLUME_ID 0x08
#define FAT_FILE_ATTR_DIRECTORY 0x10
#define FAT_FILE_ATTR_ARCHIVE   0x20

namespace fat_fs {
	struct fat_parameters {
		uint8_t sector_one[512];

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
	};

	struct fat_directory_entry {
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

		inline uint32_t start_cluster() { return (this->start_cluster_hi << 16) | this->start_cluster_lo; };
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

	class fat_fs : public vfs_fs {
		friend class fat_cluster_chain;
		fat_parameters params;

		// FAT read/write
		unsigned int allocate_cluster();
		void write_fat( uint32_t, uint32_t );
		unsigned int read_fat( uint32_t );

		// single cluster manipulation functions
		void *get_clusters( vector<uint32_t>* );
		void *get_cluster( uint32_t );
		void write_cluster( uint32_t, void* );
		uint64_t cluster_to_lba(uint32_t);

		bool update_dir_entry( vfs_directory* dir, unsigned char *shortname, fat_directory_entry *rep );
		fat_directory_entry* read_dir_entry(vfs_directory* dir, unsigned char *shortname);
		void update_node(vfs_node* node);
	public:
		//vfs_directory *base;

		// VFS interface functions
		vfs_file* create_file( unsigned char* name, vfs_directory* parent );
		vfs_directory* create_directory( unsigned char* name, vfs_directory* parent );
		void delete_file( vfs_file* file );
		void read_file( vfs_file* file, void* buffer );
		void write_file( vfs_file* file, void* buffer, size_t size);
		vfs_file* copy_file( vfs_file* file, vfs_directory* destination );
		vfs_file* move_file( vfs_file* file, vfs_directory* destination );
		vfs_directory* read_directory( vfs_directory* parent, vfs_node *child );

		fat_fs( unsigned int part_id );
	};

	class fat_cluster_chain {
		fat_fs *parent_fs;
	public:
		vector<uint32_t> clusters;

		void *read();
		void write(void* data, size_t byte_length);
		void extend(unsigned int n_clusters);
		void shrink(unsigned int n_clusters);
		size_t get_size() { return (this->clusters.count() * this->parent_fs->params.sectors_per_cluster * 512); };

		fat_cluster_chain(fat_fs *parent, uint32_t start_cluster);
		fat_cluster_chain(fat_fs *parent);
	};

	vector<fat_longname> *generate_lfn_entries(unsigned char* name);
	unsigned char* generate_longname(vector<fat_longname> lfn_entries);
	unsigned char  generate_lfn_checksum(unsigned char *primary, unsigned char *ext);
	unsigned char* generate_basisname( vfs_node* node );
};

void fat32_do_format( unsigned int part_id );
