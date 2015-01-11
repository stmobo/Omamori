#pragma once
#include "includes.h"
#include "lib/vector.h"

class vfs_fs;

struct vfs_attributes {
    bool read_only;
    bool hidden;
    uint64_t ctime = 0;
    uint64_t atime = 0;
    uint64_t mtime = 0;
    char* fstype;
};

struct vfs_node {
	unsigned char *name;
    void *fs_info;
    vfs_fs *fs;
    vfs_attributes attr;
    vfs_node* parent;
    
    vfs_node( vfs_node* p, vfs_fs *f, void* d, unsigned char* n ) : name(n), fs_info(d), fs(f), parent(p) {}
};

struct vfs_file : public vfs_node {
    uint64_t size;
    //using vfs_node::vfs_node;

    vfs_file( vfs_node* p, vfs_fs *f, void* d, unsigned char* n ) : vfs_node(p, f, d, n), size(0) {};
    vfs_file(vfs_node* cp) : vfs_node( cp->parent, cp->fs, cp->fs_info, cp->name ), size(0) {};
};

struct vfs_directory : public vfs_node {
    vector<vfs_node*> files;

    vfs_directory( vfs_node* p, vfs_fs *f, void* d, unsigned char* n ) : vfs_node(p, f, d, n) {};
	vfs_directory(vfs_node* cp) : vfs_node( cp->parent, cp->fs, cp->fs_info, cp->name ) {};
};

class vfs_fs {
	virtual vfs_file* create_file( unsigned char* name, vfs_directory* parent ) =0;
	virtual vfs_directory* create_directory( unsigned char* name, vfs_directory* parent ) =0;
	virtual void delete_file( vfs_file* file ) =0;
	virtual void read_file( vfs_file* file, void* buffer ) =0;
	virtual void write_file( vfs_file* file, void* buffer, size_t size)  =0;
	//virtual void copy_file( vfs_file* file, vfs_directory* destination ) =0;
	//virtual void move_file( vfs_file* file, vfs_directory* destination ) =0;
	virtual vfs_directory* read_directory( vfs_directory* parent, vfs_node *child ) =0;
};
