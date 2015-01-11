#pragma once
#include "includes.h"

struct vfs_attributes {
    bool read_only;
    bool hidden;
    uint64_t ctime = 0;
    uint64_t atime = 0;
    uint64_t mtime = 0;
    char* fstype;
};

struct vfs_node {
    char *name;
    void *fs_info;
    vfs_attributes attr;
    vfs_node* parent;
    
    vfs_node( vfs_node* p, void* d, char* n ) : name(n), fs_info(d), parent(p) {}
};

struct vfs_file : public vfs_node {
    uint64_t size;
    using vfs_node::vfs_node;
};

struct vfs_directory : public vfs_node {
    vector<vfs_node*> files;
    using vfs_node::vfs_node;
};
