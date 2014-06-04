// Newlib glue layer
#include <errno.h>
#include <sys/stat.h>
#include <sys/times.h>
#include "core/scheduler.h"
#include "device/ps2_keyboard.h"
#include "device/vga.h"
#undef errno
extern int errno;

extern "C" {
    char *__env[1] = { 0 };
    char **environ = __env;
    
    void _exit() {
        delete process_current;
        process_switch_immediate();
    }

    int open(const char *name, int flags, int mode){
        return -1;
    }
    
    int close(int file) {
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
        return 1;
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
        }
        return 0;
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
        }        
        return len;
    }
    
    int gettimeofday(struct timeval *tv, struct timezone *tz) {
        return get_sys_time_counter(); // note: write a CMOS driver at some point to get time/date
    }

}