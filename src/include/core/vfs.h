#pragma once
#include "includes.h"

struct vfs_attributes {
    bool read_only;
    bool hidden;
    uint64_t ctime = 0;
    uint64_t atime = 0;
    uint64_t mtime = 0;
};

struct vfs_file {
    char *name;
    vfs_attributes attr;
    void *fs_data;
};

struct vfs_directory {
    vector<vfs_file*> files;
    void *fs_data;
};