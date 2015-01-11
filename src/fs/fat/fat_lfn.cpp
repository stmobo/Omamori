/*
 * fat_lfn.cpp
 *
 *  Created on: Jan 11, 2015
 *      Author: Tatantyler
 */

#include "fs/fat/fat_fs.h"

unsigned char fat_fs::generate_lfn_checksum(unsigned char *primary, unsigned char *ext) {
	unsigned char shortname[11];
	for(int i=0;i<8;i++) {
		shortname[i] = primary[i];
	}
	for(int i=0;i<3;i++) {
		shortname[i+8] = ext[i];
	}

	unsigned char *tmp = shortname;
	uint8_t sum = 0;
	for( int i=11;i>0;i-- ) {
		sum = ( (sum&1) ? 0x80 : 0 ) + (sum>>1) + *tmp++;
	}

	return sum;
}

vector<fat_fs::fat_longname> *fat_fs::generate_lfn_entries(unsigned char* name) {
	vector<fat_longname> *list = new vector<fat_longname>;
	unsigned int len = strlen((char*)name);
	unsigned int n_emitted = 0;

	do {
		fat_longname current;

		bool do_exit = false;
		for(int i=1;i<10;i+=2) {
			if( do_exit ) {
				current.name_lo[i-1] = 0xFF;
				current.name_lo[i] = 0xFF;
			} else if( n_emitted == len ) {
				current.name_lo[i-1] = 0;
				current.name_lo[i] = 0;
				do_exit = true;
			} else {
				current.name_lo[i-1] = name[n_emitted++];
				current.name_lo[i] = 0;
			}
		}

		for(int i=1;i<12;i+=2) {
			if( do_exit ) {
				current.name_med[i-1] = 0xFF;
				current.name_med[i] = 0xFF;
			} else if( n_emitted == len ) {
				current.name_med[i-1] = 0;
				current.name_med[i] = 0;
				do_exit = true;
			} else {
				current.name_med[i-1] = name[n_emitted++];
				current.name_med[i] = 0;
			}
		}

		for(int i=1;i<4;i+=2) {
			if( do_exit ) {
				current.name_hi[i-1] = 0xFF;
				current.name_hi[i] = 0xFF;
			} else if( n_emitted == len ) {
				current.name_hi[i-1] = 0;
				current.name_hi[i] = 0;
				do_exit = true;
			} else {
				current.name_hi[i-1] = name[n_emitted++];
				current.name_hi[i] = 0;
			}
		}
		current.seq_num = (list->count()+1) | (do_exit ? 0x40 : 0 );
		current.attr = 0x0F;
		current.type = 0;
		current.checksum = 0; // do checksumming later
		list->add_end(current);
	} while( n_emitted < len );

	return list;
}

unsigned char* fat_fs::generate_longname(vector<fat_longname> lfn_entries) {
	unsigned char *name = (unsigned char*)kmalloc( (lfn_entries.count() * 13) + 1 );
	unsigned char *out = name;
	for(int seq=0;seq<lfn_entries.count();seq++) {
		for(int i=0;i<lfn_entries.count();i++) {
			if( lfn_entries[i].seq_num == seq ) {
				for(int j=0;j<10;j+=2) {
					if( (lfn_entries[i].name_lo[j] == 0) || (lfn_entries[i].name_lo[j] == 0xFF) )
						break;
					*name = lfn_entries[i].name_lo[j];
					name++;
				}
				for(int j=0;j<12;j+=2) {
					if( (lfn_entries[i].name_med[j] == 0) || (lfn_entries[i].name_med[j] == 0xFF) )
						break;
					*name = lfn_entries[i].name_med[j];
					name++;
				}
				for(int j=0;j<4;j+=2) {
					if( (lfn_entries[i].name_hi[j] == 0) || (lfn_entries[i].name_hi[j] == 0xFF) ) {
						if( seq == (lfn_entries.count()-1) ) {
							*name = '\0';
							name++;
						}
						break;
					}
					*name = lfn_entries[i].name_hi[j];
					name++;
				}
			}
		}
	}
	out[lfn_entries.count() * 13] = '\0';
	return out;
}

unsigned char* fat_fs::generate_basisname( vfs_node* node ) {
	fat_directory_entry *ent = (fat_directory_entry*)node->fs_info;

	int len = strlen((char*)node->name);
	int last_period_pos = 0;
	bool generate_numeric_tail = false;

	unsigned char new_shortname[8];

	for( signed int i=strlen((char*)node->name);i>0;i--) {
		if( node->name[i-1] == '.' ) {
			last_period_pos = i-1;
			break;
		}
	}

	if( last_period_pos == 0 )
		last_period_pos = len;

	char* stripped_longname = (char*)kmalloc(last_period_pos+1);
	int stripped_period_pos = 0;
	int j = 0;

	for(int i=0;i<len;i++) {
		switch (node->name[i]) {
			case ' ':
				generate_numeric_tail = true;
				break;
			case '.':
				if( i == last_period_pos ) {
					stripped_period_pos = j;
					stripped_longname[j++] = node->name[i];
				} else {
					generate_numeric_tail = true;
				}
				break;
			case ',':
			case '[':
			case ']':
			case ';':
			case '=':
			case '+':
				generate_numeric_tail = true;
				stripped_longname[j++] = '_';
				break;
			default:
				if( (node->name[i] <= 0x1F) || (node->name[i] >= 0x7E) ) {
					generate_numeric_tail = true;
					stripped_longname[j++] = '_';
					break;
				} else if( (node->name[i] >= 0x61) && (node->name[i] <= 0x7A) ) { // lowercase to uppercase
					stripped_longname[j++] = node->name[i]-0x20;
				} else {
					stripped_longname[j++] = node->name[i];
				}
				break;
		}
	}

	//char* basisname_primary = kmalloc(8);

	if( stripped_period_pos < 8 ) {
		for( int i=0;i<stripped_period_pos;i++) {
			new_shortname[i] = stripped_longname[i];
		}
		for( int i=stripped_period_pos;i<8;i++ ) {
			new_shortname[i] = ' ';
		}
	} else {
		generate_numeric_tail = true;
		for( int i=0;i<8;i++) {
			new_shortname[i] = stripped_longname[i];
		}
	}

	//char* basisname_extension = kmalloc(3);

	j = 0;
	for( int i=last_period_pos+1;((i<len) && (j<3));i++ ) {
		ent->ext[j++] = stripped_longname[i];
	}

	kfree(stripped_longname);

	// generate a numeric tail here
	int numeric_id = 1;
	char *test_tail = (char*)kmalloc(9);
	bool matches = false;

	do {
		// generate a test numeric tail string
		for( int i=0;i<8;i++) {
			test_tail[i] = new_shortname[i];
		}
		char *num_id_str = itoa(numeric_id);
		test_tail[7-strlen(num_id_str)] = '~';
		for(int i=0;i<strlen(num_id_str);i++) {
			test_tail[7-i] = num_id_str[strlen(num_id_str)-(i+1)];
		}
		test_tail[8] = '\0';

		vfs_directory *parent = (vfs_directory*)node->parent;

		for(unsigned int i=0; i < parent->files.count(); i++) {
			fat_directory_entry *cur = (fat_directory_entry*)parent->files.get(i)->fs_info;
			if( cur->attr != 0x0F ) { // if the current direntry is NOT a long file name....
				bool sub_match = true;
				for( int k=0;k<8;k++ ) {
					if( cur->shortname[k] != test_tail[k] ) {
						sub_match = false;
						break;
					}
				}
				if( sub_match ) {
					matches = true;
					goto __fat32_generate_tail_loop_end;
				}
			}
		}
		__fat32_generate_tail_loop_end:
		numeric_id++;
	} while( matches );

	unsigned char *ret = (unsigned char*)kmalloc(8);
	for(int i=0;i<8;i++) {
		ret[i] = test_tail[i];
	}

	delete test_tail;



	return ret;
}
