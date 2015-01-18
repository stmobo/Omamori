/*
 * vfs_main.cpp
 *
 *  Created on: Jan 17, 2015
 *      Author: Tatantyler
 */

#include "includes.h"
#include "core/vfs.h"

vector<vfs::mount_point> vfs::mounted_filesystems;
vfs_directory* vfs::vfs_root;

vfs_directory::~vfs_directory() {
	for(unsigned int i=0;i<this->files.count();i++) {
		delete this->files[i];
	}
	this->fs->cleanup_node( this );
}

// NOTE: the VFS subsystem (in src/fs/vfs) maintains node consistency itself!
// The underlying FS drivers shouldn't modify the node contents themselves when adding or deleting files/directories!

vfs::vfs_status vfs::get_file_info( unsigned char* path, vfs_node** out ) {
	vector<unsigned char*> path_components = vfs::split_path(path);

	vfs_directory* cur = vfs_root;
	for(unsigned int i=0;i<path_components.count()-1;i++) {
		bool found = false;
		for(unsigned int j=0;j<cur->files.count();j++) {
			vfs_node* cur_node = cur->files[j];
			if( strcmp(cur_node->name, path_components[i]) && (cur_node->type == vfs_node_types::directory) ) {
				cur = (vfs_directory*)cur_node;
				found = true;
				break;
			}
		}
		if(!found) {
			return vfs_status::not_found;
		}
	}

	for(unsigned int j=0;j<cur->files.count();j++) {
		vfs_node* cur_node = cur->files[j];
		if( strcmp(cur_node->name, path_components[path_components.count()-1]) ) {
			*out = cur_node;
		}
	}

	*out = NULL;
	return vfs_status::not_found;
}

vfs::vfs_status get_path_parent( unsigned char* path, vfs_directory** out ) {
	return vfs::get_file_info( vfs::get_pathstem(path), (vfs_node**)out );
}

bool vfs::file_exists( unsigned char* path ) {
	vfs_node* node;
	vfs_status stat = get_file_info(path, &node);
	if( stat != vfs_status::ok ) // includes vfs_status::not_found
		return false;
	return true;
}

vfs::vfs_status vfs::list_directory(unsigned char* path, vector<vfs_node*>* out) {
	vfs_node *node;

	vfs_status stat = vfs::get_file_info(path, &node);
	if( stat != vfs_status::ok ) {
		return stat;
	}

	if( node->type != vfs_node_types::directory ) {
		return vfs_status::incorrect_type;
	}

	vfs_directory *dir = (vfs_directory*)node;
	for(unsigned int i=0;i<dir->files.count();i++) {
		out->add_end( dir->files[i] );
	}

	return vfs_status::ok;
}

vfs::vfs_status vfs::create_directory( unsigned char* path ) {
	vfs_directory* parent;
	vfs_status stat;

	if( (stat = get_path_parent( path, &parent )) != vfs_status::ok ) {
		return stat;
	}

	vfs_node* new_node = (vfs_node*)parent->fs->create_directory( vfs::get_filename(path), parent );
	parent->files.add(new_node);

	return vfs_status::ok;
}

vfs::vfs_status vfs::delete_file( unsigned char* path ) {
	vfs_node* node;
	vfs_status stat;

	if( (stat = get_file_info( path, &node )) != vfs_status::ok ) {
		return stat;
	}

	if( node->type != vfs_node_types::file ) {
		return vfs_status::incorrect_type;
	}

	node->fs->delete_file( (vfs_file*)node );

	if( node->parent->type != vfs_node_types::directory ) {
		return vfs_status::incorrect_type;
	}
	vfs_directory *parent = (vfs_directory*)node->parent;
	for(unsigned int i=0;i<parent->files.count();i++) {
		if( parent->files[i] == node ) {
			parent->files.remove(i);
			break;
		}
	}

	return vfs_status::ok;
}

vfs::vfs_status vfs::read_file( unsigned char* path, void* buffer ) {
	vfs_node* node;
	vfs_status stat;

	if( (stat = get_file_info( path, &node )) != vfs_status::ok ) {
		return stat;
	}

	if( node->type != vfs_node_types::file ) {
		return vfs_status::incorrect_type;
	}

	node->fs->read_file((vfs_file*)node, buffer);

	return vfs_status::ok;
}

vfs::vfs_status vfs::write_file(unsigned char* path, void* buffer, size_t size) {
	vfs_status stat;

	vfs_file *file;
	stat = get_file_info( path, (vfs_node**)&file );
	if( stat == vfs_status::not_found ) {
		// file does not exist, create it
		vfs_directory *parent;

		if( (stat = get_path_parent( path, &parent )) != vfs_status::ok ) {
			return stat;
		}

		file = parent->fs->create_file( get_filename(path), parent );
		parent->files.add_end( (vfs_node*)file );
	} else if( stat == vfs_status::ok ) {
		file->fs->write_file(file, buffer, size);

		return vfs_status::ok;
	}
	return stat;
}

vfs::vfs_status vfs::copy_file( unsigned char* to, unsigned char* from ) {
	vfs_node* to_node;
	vfs_node* from_node;
	vfs_status stat;

	if( (stat = get_file_info( from, &from_node )) != vfs_status::ok ) {
		return stat;
	}

	if( from_node->type != vfs_node_types::file ) {
		return vfs_status::incorrect_type;
	}

	if( (stat = get_file_info( to, &to_node )) != vfs_status::ok ) {
		return stat;
	}

	if( to_node->type != vfs_node_types::directory ) {
		return vfs_status::incorrect_type;
	}

	if( to_node->fs == from_node->fs ) {
		vfs_file* new_file = to_node->fs->copy_file( (vfs_file*)from_node, (vfs_directory*)to_node );
		vfs_directory* to_parent = (vfs_directory*)to_node;
		to_parent->files.add_end((vfs_node*)new_file);
		return vfs_status::ok;
	} else {
		vfs_file* src = (vfs_file*)from_node;
		void* tmp = kmalloc(src->size);
		from_node->fs->read_file(src, tmp);

		vfs_directory *dst = (vfs_directory*)to_node;
		return write_file( to, tmp, src->size );
	}

	return vfs_status::ok;
}

vfs::vfs_status vfs::move_file( unsigned char* to, unsigned char* from ) {
	vfs_node* to_node;
	vfs_node* from_node;
	vfs_status stat;

	if( (stat = get_file_info( from, &from_node )) != vfs_status::ok ) {
		return stat;
	}

	if( from_node->type != vfs_node_types::file ) {
		return vfs_status::incorrect_type;
	}

	if( (stat = get_file_info( to, &to_node )) != vfs_status::ok ) {
		return stat;
	}

	if( to_node->type != vfs_node_types::directory ) {
		return vfs_status::incorrect_type;
	}

	if( to_node->fs == from_node->fs ) {
		vfs_file* new_file = to_node->fs->move_file( (vfs_file*)from_node, (vfs_directory*)to_node );
		vfs_directory* to_parent = (vfs_directory*)to_node;
		to_parent->files.add_end((vfs_node*)new_file);

		vfs_directory *from_parent;
		if( (stat = get_path_parent( from, &from_parent )) != vfs_status::ok ) {
			return stat;
		}
		for(unsigned int i=0;i<from_parent->files.count();i++) {
			if( from_parent->files[i] == from_node ) {
				from_parent->files.remove(i);
				break;
			}
		}

		return vfs_status::ok;
	} else {
		vfs_file* src = (vfs_file*)from_node;
		void* tmp = kmalloc(src->size);
		from_node->fs->read_file(src, tmp);

		vfs_directory *dst = (vfs_directory*)to_node;
		if( (stat = write_file( to, tmp, src->size )) != vfs_status::ok ) {
			return stat;
		}
		return delete_file( from );
	}

	return vfs_status::ok;
}

vfs::vfs_status vfs::mount( vfs_fs* fs, unsigned char* path ) {
	if( file_exists(path) ) {
		return vfs_status::already_exists;
	}

	vfs_directory* parent;
	vfs_status stat;

	if( (stat = get_path_parent( path, &parent )) != vfs_status::ok ) {
		return stat;
	}

	parent->files.add_end( (vfs_node*)fs->base );
	fs->base->name = vfs::get_filename(path);
	fs->base->parent = (vfs_node*)parent;
	vfs::mount_point mount_entry;
	mount_entry.filesystem = fs;
	mount_entry.path = (unsigned char*)kmalloc(strlen(path)+1);
	for(unsigned int i=0;i<strlen(path);i++) {
		mount_entry.path[i] = path[i];
	}
	mount_entry.path[strlen(path)] = '\0';
	mount_entry.parent = parent;

	vfs::mounted_filesystems.add_end(mount_entry);

	return vfs_status::ok;
}

vfs::vfs_status vfs::unmount( unsigned char* path ) {
	if( !file_exists(path) ) {
		return vfs_status::not_found;
	}

	vfs_directory* parent;
	vfs_status stat;

	if( (stat = get_path_parent( path, &parent )) != vfs_status::ok ) {
		return stat;
	}

	vfs::mount_point mount_entry;
	for(unsigned int i=0;i<vfs::mounted_filesystems.count();i++) {
		if( strcmp( vfs::mounted_filesystems[i].path, path ) ) {
			mount_entry = vfs::mounted_filesystems[i];
			vfs::mounted_filesystems.remove(i);
			break;
		}
	}

	for(unsigned int i=0;i<parent->files.count();i++) {
		if( parent->files[i] == (vfs_node*)mount_entry.filesystem->base ) {
			parent->files.remove(i);
			break;
		}
	}

	delete mount_entry.filesystem;
	delete mount_entry.path;

	return vfs_status::ok;
}
