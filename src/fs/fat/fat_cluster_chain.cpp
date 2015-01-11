/*
 * fat_cluster_chain.cpp
 *
 *  Created on: Jan 11, 2015
 *      Author: Tatantyler
 */

#include "fs/fat/fat_fs.h"
#include "core/io.h"

fat_fs::fat_cluster_chain::fat_cluster_chain( fat_fs *parent) {
	this->parent_fs = parent;

	uint32_t new_cluster = this->parent_fs->allocate_cluster();
	this->clusters.add_end(new_cluster);
	this->parent_fs->write_fat(new_cluster, 0x0FFFFFF8);
}

fat_fs::fat_cluster_chain::fat_cluster_chain( fat_fs *parent, uint32_t start_cluster) {
	this->parent_fs = parent;

	uint32_t current = start_cluster;
	uint32_t next = 0;
	uint32_t fat_sector = this->parent_fs->params.n_reserved_sectors + ( (current*4) / 512 );
	bool do_read = true;

	kprintf("fat32: reading cluster chain starting from cluster %u.\n", start_cluster);

	void *buf = kmalloc(512);
	do {
		uint32_t fat_offset = (current*4) % 512;

		if( do_read ) {
			io_read_partition( this->parent_fs->params.part_no, buf, fat_sector*512, 512 );
			do_read = false;
		}

		uint8_t* cluster = (uint8_t*)buf;

		next = *((uint32_t*)(cluster+fat_offset)) & 0x0FFFFFFF;
		if( next != 0 ) // if this is actually allocated....
			this->clusters.add_end( current );

		kprintf("fat32: current cluster=%u / %#x\n", current, current);
		kprintf("fat32: data at %#p\n", buf);

		/*
		logger_flush_buffer();
		system_halt;
		*/

		if( ( (next != 0) && !( (next & 0x0FFFFFFF) >= 0x0FFFFFF8 ) ) && ( (this->parent_fs->params.n_reserved_sectors + ( (current*4) / 512 )) != (this->parent_fs->params.n_reserved_sectors + ( (next*4) / 512 )) ) ) { // if this isn't the end of the chain and the next sector to read is different...
			do_read = true;
			fat_sector = (this->parent_fs->params.n_reserved_sectors + ( (next*4) / 512 ));
		}

		current = next;

		kprintf("fat32: next cluster=%u / %#x\n", next, next);
	} while( (next != 0) && !( (next & 0x0FFFFFFF) >= 0x0FFFFFF8 ) );
	kfree(buf);
}

void fat_fs::fat_cluster_chain::write(void* data, size_t byte_length) {
	uint64_t n_clusters = this->clusters.count();
	uint32_t new_clusters = (byte_length / 512) / this->parent_fs->params.sectors_per_cluster;

	if( n_clusters > new_clusters ) { // shrink the file
		uint32_t diff = n_clusters - new_clusters;
		this->shrink( diff );
	} else if( n_clusters < new_clusters ) { // extend the file
		uint32_t diff = new_clusters - n_clusters;
		this->extend( diff );
	}  // file cluster chain length stays the same

	void* cur = data;
	for( int i=0;i<(n_clusters-1);i++ ) { // write out individual clusters now
		this->parent_fs->write_cluster( this->clusters.get(i), data );
		uintptr_t ptr_int = (uintptr_t)cur;
		ptr_int += (this->parent_fs->params.sectors_per_cluster * 512);
		cur = (void*)ptr_int; // cur = cur + sectors_per_cluster * 512
	}

	void *last_cluster = kmalloc( this->parent_fs->params.sectors_per_cluster * 512 );
	size_t overflow = byte_length % ( this->parent_fs->params.sectors_per_cluster * 512 );
	memcpy( last_cluster, cur, overflow );

	this->parent_fs->write_cluster( this->clusters.get(n_clusters-1), data );

	kfree(last_cluster);
}

void* fat_fs::fat_cluster_chain::read() {
	void *buf = kmalloc( this->clusters.count() * this->parent_fs->params.sectors_per_cluster * 512 );
	void *out = buf;
	uintptr_t out_int = (uintptr_t)buf;
	for( int i=0;i<this->clusters.count();i++ ) {
		io_read_partition( this->parent_fs->params.part_no, buf, this->parent_fs->cluster_to_lba( this->clusters.get(i) )*512, this->parent_fs->params.sectors_per_cluster * 512 );
		out_int += (this->parent_fs->params.sectors_per_cluster * 512);
		buf = (void*)out_int;
	}
	return out;
}

void fat_fs::fat_cluster_chain::extend( unsigned int n_clusters ) {
	uint32_t last_cluster = this->clusters.get(this->clusters.count()-1);
	for(int i=0;i<n_clusters;i++) {
		uint32_t next_cluster = this->parent_fs->allocate_cluster();
		this->parent_fs->write_fat(last_cluster, next_cluster);
		this->clusters.add_end(next_cluster);
		last_cluster = next_cluster;
	}
	this->parent_fs->write_fat(last_cluster, 0x0FFFFFF8);
}

void fat_fs::fat_cluster_chain::shrink( unsigned int n_clusters ) {
	uint32_t last_cluster = this->clusters.remove_end();
	for(int i=0;i<n_clusters-1;i++) {
		this->parent_fs->write_fat(last_cluster, 0);
		last_cluster = this->clusters.remove_end();
	}
	this->parent_fs->write_fat(last_cluster, 0x0FFFFFF8);
}
