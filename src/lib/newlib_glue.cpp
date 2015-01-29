// Newlib glue layer
#include <errno.h>
#include <sys/stat.h>
#include <sys/times.h>
#include "core/scheduler.h"
#include "device/ps2_keyboard.h"
#include "device/vga.h"
#include "core/vfs.h"
#undef errno
extern int errno;

struct file_descriptor {
	vfs_node* node;
	unsigned char* name;
	process_ptr process;
};

int vfs_err_to_errno( vfs::vfs_status st ) {
	switch(st) {
	case vfs::vfs_status::already_exists:
		return EEXIST;
	case vfs::vfs_status::incorrect_type:
		return EINVAL;
	case vfs::vfs_status::not_found:
		return ENOENT;
	default:
	case vfs::vfs_status::unknown_error:
		return EIO;
	}
}

extern "C" {
    char *__env[1] = { 0 };
    char **environ = __env;
    
    void _exit() {
        delete process_current;
        process_switch_immediate();
    }

    vector<file_descriptor*> open_files(2);

    int open(const char *name, int flags, int mode){
    	vfs_node* node = NULL;
        vfs::vfs_status fop_stat = vfs::get_file_info( (unsigned char*)const_cast<char*>(name), &node );

        if( fop_stat == vfs::vfs_status::ok ) {
        	file_descriptor *file = new file_descriptor;
        	file->node = node;
        	file->process = process_current;
        	file->name = (unsigned char*)kmalloc(strlen(const_cast<char*>(name))+1);
        	for(unsigned int i=0;i<strlen(const_cast<char*>(name));i++) {
        		file->name[i] = name[i];
        	}
        	file->name[strlen(const_cast<char*>(name))] = '\0';

        	open_files.add_end(file);

        	return open_files.count()-1;
        } else {
        	errno = vfs_err_to_errno(fop_stat);
			return -1;
        }
    }
    
    int close(int file) {
        if( open_files[file] != NULL ) {
        	file_descriptor *f = open_files[file];

        	if( process_current != f->process.raw_ptr() ) {
        		errno = EACCES;
        		return -1;
        	}

        	open_files.remove(file);
        	return 0;
        }
        errno = EBADF;
        return -1;
    }

    int execve(char* name, char **argv, char **env) {
        errno=ENOMEM;
        return -1;
    }

    uint32_t fork() {
        return syscall(1, 0,0,0,0,0);
    }

    int fstat(int file, struct stat *st) {
        st->st_mode = S_IFCHR;
        return 0;
    }

    int stat(const char *file, struct stat *st) {
        st->st_mode = S_IFCHR;
        return 0;
    }

    int getpid() {
        return process_current->id;
    }

    int isatty(int file) {
        if( (file == 0) || (file == 1) ) {
        	return 1;
        }
    }

    int kill(int pid, int sig){
        errno=EINVAL;
        return(-1);
    }

    int link(char* oldfile, char* newname) {
        errno = EMLINK;
        return -1;
    }
    
    int unlink(char *name){
      errno=ENOENT;
      return -1; 
    }

    int lseek(int file, int ptr, int dir) {
        return 0;
    }

    int read(int file, char *ptr, int len) {
        if( file == 0 ) {
            unsigned int n_read = 0;
            while(true) {
                unique_ptr<ps2_keypress> kp;
                kp = ps2_keyboard_get_keystroke();
                if(!kp->released) {
                    if(kp->key == KEY_Enter) {
                        terminal_putchar('\n');
                        break;
                    } else if(kp->key == KEY_Bksp && n_read > 0) {
                        terminal_backspace();
                        n_read--;
                    } else if(kp->is_ascii) {
                        ptr[n_read++] = kp->character;
                        terminal_putchar(kp->character);
                        if( n_read == len )
                            break;
                    }
                }
            }
            return n_read;
        } else if( file != 1 ) {
        	if( open_files[file] != NULL ) {
        		if( open_files[file]->node->type == vfs_node_types::directory ) {
        			errno = EISDIR;
        			return -1;
        		}

        		vfs_file* f = (vfs_file*)open_files[file]->node;
        		void* buf = kmalloc( f->size );

        		vfs::vfs_status fop_stat = vfs::read_file( open_files[file]->name, buf );
        		if( fop_stat != vfs::vfs_status::ok ) {
        			errno = vfs_err_to_errno(fop_stat);
        			return -1;
        		}

        		if( f->size > len ) {
        			memcpy( (void*)ptr, buf, len );
        			return len;
        		} else {
        			memcpy( (void*)ptr, buf, f->size );
        			return f->size;
        		}
        	} else {
        		errno = EBADF;
        		return -1;
        	}
        }
        errno = EBADF;
        return -1;
    }

    caddr_t sbrk(int increase) {
        if( (process_current->break_val+increase) >= (((0xC0000000-1)-(PROCESS_STACK_SIZE*0x1000))&0xFFFFF000) ) {
            kprintf("Process %u died: Stack / Heap collision\n");
            _exit();
        }
        
        uint32_t old_break = process_current->break_val;
        process_current->break_val += increase;
        return (caddr_t)old_break;
    }

    void calc_ctime( process* proc, struct tms *buf ) {
        buf->tms_cutime += proc->times.prog_exec;
        buf->tms_cstime += proc->times.sysc_exec;
        for( unsigned int i=0;i<proc->children.length();i++ ) {
            calc_ctime( proc->children[i], buf );
        }
    }

    clock_t times( struct tms *buf ) {
        calc_ctime( process_current, buf );
        return (clock_t)get_sys_time_counter();
    }

    int wait(int *status) {
        if( process_current->children.length() == 0 ) {
            errno = ECHILD;
            return -1;
        }
        while(true) {
            for( unsigned int i=0;i<process_current->children.length();i++ ) {
                process *current = process_current->children[i];
                if( current->state == process_state::dead ) {
                    *status = current->return_value;
                    return 0;
                }
            }
            process_switch_immediate();
        }
    }

    int write(int file, char *ptr, int len){
        if( file == 1 ) {
            terminal_writestring( ptr, len );
        } else if( file == 2 ) {
            terminal_writestring( "err: " );
            terminal_writestring( ptr, len );
        } else {
        	if( open_files[file]->node->type == vfs_node_types::directory ) {
				errno = EISDIR;
				return -1;
			}

			vfs::vfs_status fop_stat = vfs::write_file( open_files[file]->name, (void*)ptr, len );
			if( fop_stat == vfs::vfs_status::ok ) {
				return len;
			} else {
				errno = vfs_err_to_errno(fop_stat);
				return -1;
			}
        }
        return len;
    }
    
    int gettimeofday(struct timeval *tv, struct timezone *tz) {
        return get_sys_time_counter(); // note: write a CMOS driver at some point to get time/date
    }

}
