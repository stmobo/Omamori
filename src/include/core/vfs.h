#pragma once
#include "includes.h"
#include "lib/vector.h"

struct vfs_attributes {
    bool read_only;
    bool hidden;
    uint64_t ctime = 0;
    uint64_t atime = 0;
    uint64_t mtime = 0;
    char* fstype;
};

enum class vfs_node_types {
	unknown,
	file,
	directory,
	// symlink?
};

class vfs_fs;

struct vfs_node {
	unsigned char *name;
    void *fs_info;
    vfs_fs *fs;
    vfs_attributes attr;
    vfs_node* parent;
    
    vfs_node_types type;

    vfs_node( vfs_node* p, vfs_fs *f, void* d, unsigned char* n ) : name(n), fs_info(d), fs(f), parent(p) { this->type = vfs_node_types::unknown; };
    virtual ~vfs_node() {};
};

struct vfs_file;

struct vfs_directory : public vfs_node {
    vector<vfs_node*> files;

    vfs_directory( vfs_node* p, vfs_fs *f, void* d, unsigned char* n ) : vfs_node(p, f, d, n) { this->type = vfs_node_types::directory; };
	vfs_directory(vfs_node* cp) : vfs_node( cp->parent, cp->fs, cp->fs_info, cp->name ) { this->type = vfs_node_types::directory; };
	~vfs_directory();
};

class vfs_fs {
public:
	virtual vfs_file* create_file( unsigned char* name, vfs_directory* parent ) =0;
	virtual vfs_directory* create_directory( unsigned char* name, vfs_directory* parent ) =0;
	virtual void delete_file( vfs_file* file ) =0;
	virtual void read_file( vfs_file* file, void* buffer ) =0;
	virtual void write_file( vfs_file* file, void* buffer, size_t size)  =0;
	virtual vfs_file* copy_file( vfs_file* file, vfs_directory* destination ) =0;
	virtual vfs_file* move_file( vfs_file* file, vfs_directory* destination ) =0;
	virtual vfs_directory* read_directory( vfs_directory* parent, vfs_node *child ) =0;
	virtual void cleanup_node( vfs_node* node ) { kfree( node->fs_info ); };

	virtual ~vfs_fs() { delete this->base; }

	vfs_directory *base;
};

struct vfs_file : public vfs_node {
    uint64_t size;

    vfs_file( vfs_node* p, vfs_fs *f, void* d, unsigned char* n ) : vfs_node(p, f, d, n), size(0) { this->type = vfs_node_types::file; };
    vfs_file(vfs_node* cp) : vfs_node( cp->parent, cp->fs, cp->fs_info, cp->name ), size(0) { this->type = vfs_node_types::file; };
    ~vfs_file() { this->fs->cleanup_node( this ); };
};

namespace vfs {
	struct mount_point {
		vfs_fs* filesystem;
		unsigned char* path;
		vfs_directory* parent;
	};

	enum class vfs_status {
		ok,
		not_found,
		already_exists,
		incorrect_type,
		unknown_error,
	};

	vfs_status get_file_info( unsigned char* path, vfs_node** out );
	vfs_status list_directory(unsigned char* path, vector<vfs_node*>* out );
	vfs_status create_directory( unsigned char* path );
	vfs_status delete_file( unsigned char* path );
	vfs_status read_file( unsigned char* path, void* buffer );
	vfs_status write_file( unsigned char* path, void* buffer, size_t size);
	vfs_status copy_file( unsigned char* to, unsigned char* from );
	vfs_status move_file( unsigned char* to, unsigned char* from );

	vector<unsigned char*> split_path( unsigned char* path );
	unsigned char* get_filename( unsigned char* path );
	unsigned char* get_pathstem( unsigned char* path );

	vfs_status mount( vfs_fs* fs, unsigned char* path );
	vfs_status unmount( unsigned char* path );

	bool file_exists( unsigned char* path );

	const char* status_description( vfs_status stat );

	extern vector<mount_point*> mounted_filesystems;
	extern vfs_directory* vfs_root;
};
