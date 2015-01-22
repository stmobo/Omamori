/*
 * vfs_path.cpp
 *
 *  Created on: Jan 17, 2015
 *      Author: Tatantyler
 */

// path handling support routines

#include "includes.h"
#include "core/vfs.h"
#include "lib/vector.h"

vector<unsigned char*> vfs::split_path(unsigned char* path) {
	vector<unsigned char*> ret;

	vector<unsigned char> tmp;
	unsigned char* cur = path;
	while(*cur != '\0') {
		if( *cur == '\\' || *cur == '/' ) {
			if(tmp.count() > 0) {
				unsigned char* tmp2 = (unsigned char*)kmalloc(tmp.count());
				for(unsigned int i=0;i<tmp.count();i++) {
					tmp2[i] = tmp[i];
				}
				ret.add_end(tmp2);
				tmp.clear();
			}
		} else {
			tmp.add_end(*cur);
		}
		cur++;
	}

	if(tmp.count() > 0) {
		unsigned char* tmp2 = (unsigned char*)kmalloc(tmp.count());
		for(unsigned int i=0;i<tmp.count();i++) {
			tmp2[i] = tmp[i];
		}
		ret.add_end(tmp2);
	}

	return ret;
}

// returns the last part of a filename
// "/path/to/a/file.txt" -> "file.txt"
unsigned char* vfs::get_filename( unsigned char* path ) {
	vector<unsigned char> tmp;

	for(signed int i=strlen(path)-1;i>=0;i--) {
		if( path[i] == '\\' || path[i] == '/' ) {
			break;
		}
		tmp.add( path[i] );
	}

	unsigned char* out = (unsigned char*)kmalloc(tmp.count()+1);
	for(unsigned int i=0;i<tmp.count();i++) {
		out[i] = tmp[i];
	}
	out[tmp.count()] = '\0';

	return out;
}

// returns the part of the path that is *not* the filename
// does not include the ending slash:
// "/path/to/a/file.txt" -> "/path/to/a"
// also should work on directories:
// "/path/to/a/directory" -> "/path/to/a"
unsigned char* vfs::get_pathstem( unsigned char* path ) {
	unsigned int stem_start = 0;
	bool found = false;
	for(signed int i=strlen(path)-1;i>=0;i--) {
		if( path[i] == '\\' || path[i] == '/' ) {
			stem_start = i;
			found = true;
			break;
		}
	}
	if( !found ) {
		//stem_start = strlen(path);

		unsigned char *out = (unsigned char*)kmalloc(1);
		*out = '\0';
		return out;
	}

	unsigned char *out = (unsigned char*)kmalloc(stem_start+1);
	for(unsigned int i=0;i<stem_start;i++) {
		out[i] = path[i];
	}
	out[stem_start] = '\0';

	return out;
}

const char* vfs::status_description(vfs::vfs_status stat) {
	switch( stat ) {
	case vfs_status::ok:
		return "OK";
	case vfs_status::not_found:
		return "Not found";
	case vfs_status::already_exists:
		return "Already exists";
	case vfs_status::incorrect_type:
		return "Incorrect type";
	case vfs_status::unknown_error:
		return "Unknown error";
	default:
		return "Unknown error type";
	}

	return "Unknown error type";
}
