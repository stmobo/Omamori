#include "includes.h"
#include "arch/x86/multitask.h"
#include "arch/x86/apic.h"
#include "core/acpi.h"
#include "core/scheduler.h"
#include "device/ahci.h"
#include "device/ata.h"
#include "device/pci.h"
#include "device/ps2_controller.h"
#include "device/ps2_keyboard.h"
#include "device/serial.h"
#include "device/vga.h"
#include "core/vfs.h"
#include "core/k_worker_thread.h"
#include "fs/fat/fat_fs.h"
#include "fs/iso9660/iso9660.h"

extern "C" {
    #include "lua.h"
    #include "lualib.h"
    #include "lauxlib.h"
}

void test_func(void* n) {
    terminal_writestring("atexit() works.\n");
}

static int lua_writeout_proxy(lua_State *st) {
    int n_args = lua_gettop(st);
    unsigned int ret = 0;
    for(int i=1;i<=n_args;i++) {
        if( lua_isstring(st, i) ) {
            const char *out = lua_tostring(st, i);
            terminal_writestring(const_cast<char*>(out));
            ret += strlen(const_cast<char*>(out));
        }
    }
    terminal_putchar('\n');
    lua_pushnumber(st, ret);
    return 1;
}

unsigned int k_worker_thread_test() {
	kprintf("k_worker_thread did a thing\n");
	return 0;
}

void test_process_1() {   
    kprintf("Initializing serial logging.\n");
    initialize_serial();
    
    logger_initialize();
    kprintf("Initialized kernel logger process.\n");
    
    terminal_writestring("Initializing ACPI.\n");
    initialize_acpi();

    kprintf("Initializing APICs.\n");
	logger_flush_buffer();
	initialize_apics();
	logger_flush_buffer();
	system_halt;

    kprintf("Starting kernel worker thread.\n");
    k_work::start();
    
    kprintf("Initializing PS/2 controller.\n");
    ps2_controller_init();
    
    kprintf("Initializing PS/2 keyboard.\n");
    ps2_keyboard_initialize();
    
    kprintf("Initializing I/O.\n");
    io_initialize();
    
    kprintf("Initializing PCI.\n");
    pci_check_all_buses();
    
    kprintf("Initializing ATA storage.\n");
    ata::initialize();
    
    io_detect_disk( io_get_disk( 1 ) );

    kprintf("Scheduling work...\n");
    logger_flush_buffer();
    k_work::work* wk = k_work::schedule( &k_worker_thread_test );
	kprintf("Test work function returned: %u\n", wk->wait());

    //logger_flush_buffer();
	//system_halt;

    uint32_t child_pid = fork();
    if( child_pid == -1 ) {
        kprintf("Whoops, something went wrong with fork!");
    } else if( child_pid == 0 ) {
        kprintf("Hello from (child) process %u!\n", (unsigned int)process_current->id);
        /*
        set_message_listen_status( "keypress", true );
        while(true) {
            unique_ptr<ps2_keypress> kp;
            kp = ps2_keyboard_get_keystroke();
            if( !kp->released ) {
                if( kp->key == KEY_CurUp ) {
                    kprintf("Process %u: Up!\n", process_current->id);
                } else if( kp->key == KEY_CurDown ) {
                    kprintf("Process %u: Down!\n", process_current->id);
                /*} else if( kp->key == KEY_Enter ) {
                    kprintf("Process %u: Forcing ATA cache flush...\n", process_current->id);
                    ata_do_cache_flush(); *//*
                }
            }
        }
        */
    } else {
        kprintf("Hello from (parent) process %u!\n", process_current->id);
        kprintf("Starting lua interpretation.\n");
        lua_State *st = luaL_newstate();
        //kprintf("Opened state..\n");
        
        luaL_openlibs( st );
        //kprintf("Opened libs..\n");
        lua_register( st, "writeout", lua_writeout_proxy );
        //kprintf("Registered writeout..\n");
        int stat = luaL_loadstring( st, "writeout(\"hello from lua version \",_VERSION,\"!\") return 0xC0DE" );
        //kprintf("Loaded test program..\n");
        
        if( stat == 0 ) { // was it loaded successfully?
            //kprintf("Performing call..\n");
            stat = lua_pcall( st, 0, 1, 0 );
            if( stat == 0 ) { // get return values
                int retval = lua_tonumber(st, -1);
                kprintf("Test program returned successfully, returned value: 0x%x\n", retval);
                lua_pop(st, 1);
            }
        }
        if( stat != 0 ) { // was there an error?
            const char *err = lua_tostring(st, -1);
            kprintf("lua-error: %s\n", err);
            lua_pop(st, 1);
        }
        lua_close(st);
        //kprintf("Closed state..\n");
        
        kprintf("Initializing AHCI.\n");
        ahci_initialize();

        // format partition 1 as FAT (using our own code):
        //fat32_do_format( 1 );
        
        const char* test_file_name = "test2.txt";
        char *test_file_data = "Hello world! This is a test file!";

        fat_fs::fat_fs f( 1 );
		iso9660::iso9660_fs f2(2);
		vfs::vfs_root = f.base;

		vfs::vfs_status fop_stat = vfs::mount(&f2, (unsigned char*)(const_cast<char*>("/cd")));
		if( fop_stat != vfs::vfs_status::ok ) {
			kprintf("FS mount failed with error: %s.\n", vfs::status_description(fop_stat));
		} else {
			kprintf("FS mount succeeded.\n");
		}

		while(true) {
			kprintf(">");
			logger_flush_buffer();

			unsigned int len;
			char* line = ps2_keyboard_readline( &len );

			unsigned int cmd_len = 0;
			unsigned int arg1_start = 0;
			unsigned int arg1_end = 0;
			unsigned int arg2_start = 0;

			bool found_cmd = false;
			bool found_arg1 = false;
			bool stopped_arg1 = false;
			bool found_arg2 = false;

			for(unsigned int i=0;i<len;i++) {
				if(line[i] == ' ') {
					if( !found_cmd ) {
						cmd_len = i;
						found_cmd = true;
					} else if( found_arg1 && !stopped_arg1 ) {
						arg1_end = i;
						stopped_arg1 = true;
					}
				} else {
					if( found_cmd && !found_arg1 ) {
						arg1_start = i;
						found_arg1 = true;
					} else if( found_arg1 && stopped_arg1 ) {
						arg2_start = i;
						found_arg2 = true;
						break;
					}
				}
			}


			if( found_cmd ) {
				char* cmd = NULL;
				char* arg1 = NULL;
				char* arg2 = NULL;
				cmd = (char*)kmalloc( cmd_len+1 );

				for(unsigned int i=0;i<cmd_len;i++) {
					cmd[i] = line[i];
				}
				cmd[cmd_len] = '\0';

				if( found_arg1 ) {
					if( stopped_arg1 ) {
						arg1 = (char*)kmalloc( (arg1_end - arg1_start)+1 );

						for(unsigned int i=arg1_start;i<arg1_end;i++) {
							arg1[ i - arg1_start ] = line[i];
						}
						arg1[ arg1_end - arg1_start ] = '\0';

						if( found_arg2 ) {
							arg2 = (char*)kmalloc( (len - arg2_start)+1 );
							for(unsigned int i=arg2_start;i<len;i++) {
								arg2[i-arg2_start] = line[i];
							}
							arg2[len-arg2_start] = '\0';
						}
					} else {
						arg1 = (char*)kmalloc( (len - arg1_start)+1 );
						for(unsigned int i=arg1_start;i<len;i++) {
							arg1[ i - arg1_start ] = line[i];
						}
						arg1[ len - arg1_start ] = '\0';
					}
				}

				if( arg1 != NULL ) {
					if( strcmp( cmd, const_cast<char*>("ls") ) || strcmp( cmd, const_cast<char*>("list") ) ) {
						vector<vfs_node*> dls;
						vfs::vfs_status fop_stat = vfs::list_directory( (unsigned char*)arg1, &dls );
						if( fop_stat != vfs::vfs_status::ok ) {
							kprintf("FS list failed with error: %s\n", vfs::status_description(fop_stat));
						} else {
							kprintf("FS list succeeded.\n");
						}
						kprintf( "Directory listing:\n" );
						for( unsigned int i=0;i<dls.count();i++) {
							kprintf( "%s\n", dls[i]->name );
						}
					} else if( (arg2 != NULL) && strcmp( cmd, const_cast<char*>("write") ) ) {
						vfs::vfs_status fop_stat = vfs::write_file( (unsigned char*)arg1, (void*)arg2, strlen(arg2)+1);
						if( fop_stat != vfs::vfs_status::ok ) {
							kprintf("FS write failed with error: %s\n", vfs::status_description(fop_stat));
						} else {
							kprintf("FS write succeeded.\n");
						}
					} else if( strcmp( cmd, const_cast<char*>("read") ) ) {
						vfs_node* node = NULL;
						vfs::vfs_status fop_stat = vfs::get_file_info( (unsigned char*)arg1, &node );
						if( fop_stat != vfs::vfs_status::ok ) {
							kprintf("FS get info failed with error: %s\n", vfs::status_description(fop_stat));
						} else {
							kprintf("FS get info succeeded.\n");
						}

						if( node != NULL ) {
							if( node->type != vfs_node_types::file ) {
								kprintf("FS read failed with error: incorrect type\n");
								continue;
							}
						}

						vfs_file* fn = (vfs_file*)node;
						void* buffer = kmalloc( fn->size );

						fop_stat = vfs::read_file( (unsigned char*)arg1, buffer );
						if( fop_stat != vfs::vfs_status::ok ) {
							kprintf("FS read failed with error: %s\n", vfs::status_description(fop_stat));
						} else {
							kprintf("FS read succeeded.\n");
						}

						kprintf("file data: %s\n", (unsigned char*)buffer );
					} else if( strcmp( cmd, const_cast<char*>("delete") ) || strcmp( cmd, const_cast<char*>("rm") ) ) {
						vfs::vfs_status fop_stat = vfs::delete_file( (unsigned char*)arg1 );
						if( fop_stat != vfs::vfs_status::ok ) {
							kprintf("FS delete failed with error: %s\n", vfs::status_description(fop_stat));
						} else {
							kprintf("FS delete succeeded.\n");
						}
					} else if( ( arg2 != NULL ) && (strcmp( cmd, const_cast<char*>("copy") ) || strcmp( cmd, const_cast<char*>("cp") ) ) ) {
						vfs::vfs_status fop_stat = vfs::copy_file( (unsigned char*)arg2, (unsigned char*)arg1 );
						if( fop_stat != vfs::vfs_status::ok ) {
							kprintf("FS copy failed with error: %s\n", vfs::status_description(fop_stat));
						} else {
							kprintf("FS copy succeeded.\n");
						}
					} else if( ( arg2 != NULL ) && (strcmp( cmd, const_cast<char*>("move") ) || strcmp( cmd, const_cast<char*>("mv") ) ) ) {
						vfs::vfs_status fop_stat = vfs::move_file( (unsigned char*)arg2, (unsigned char*)arg1 );
						if( fop_stat != vfs::vfs_status::ok ) {
							kprintf("FS move failed with error: %s\n", vfs::status_description(fop_stat));
						} else {
							kprintf("FS move succeeded.\n");
						}
					}
				}
			}
		}

		/*

        vfs_directory *root = f.base;
        vfs_file *test_file = NULL;
        kprintf( "Directory listing:\n" );
        for( unsigned int i=0;i<root->files.count();i++) {
            vfs_node *fn = root->files[i];
            kprintf( "* %u - %s\n", i, fn->name );
            if( strcmp(const_cast<char*>(test_file_name), (char*)(fn->name), 0) ) {
            	test_file = new vfs_file(const_cast<vfs_node*>(fn));
            }
        }

        if(test_file == NULL) {
        	test_file = f.create_file( (unsigned char*)(test_file_name), f.base );
			kprintf("test file created\n");
        	test_file->size = strlen(test_file_data)+1;

        	f.write_file(test_file, (void*)test_file_data, strlen(test_file_data)+1);
        	kprintf("File written out to %s\n", test_file_name);
        } else {
        	//f.write_file(test_file, (void*)test_file_data, strlen(test_file_data)+1);
			//kprintf("File written out to %s\n", test_file_name);

			void *readback = kmalloc(strlen(test_file_data)+1);
			f.read_file(test_file, readback);

			char* readback_data = (char*)readback;
			kprintf("Readback data: %s (%#p)\n", readback_data, readback);
        }


		unsigned char f2_mountpoint[] = { '/', 'c', 'd', '\0' };
		root = f2.base;
		kprintf( "Directory listing (ISO 9660):\n" );
		for( unsigned int i=0;i<root->files.count();i++) {
			vfs_node *fn = root->files[i];
			kprintf( "* %u - %s\n", i, fn->name );
		}

		vfs::vfs_status fop_stat = vfs::mount(&f2, f2_mountpoint);
		if( fop_stat != vfs::vfs_status::ok ) {
			kprintf("FS mount failed with error: %s.\n", vfs::status_description(fop_stat));
		} else {
			kprintf("FS mount succeeded.\n");
		}

		vector<vfs_node*> dls;
		fop_stat = vfs::list_directory( (unsigned char*)(const_cast<char*>("/")), &dls );
		if( fop_stat != vfs::vfs_status::ok ) {
			kprintf("FS list failed with error: %s\n", vfs::status_description(fop_stat));
		} else {
			kprintf("FS list succeeded.\n");
		}

		kprintf( "Directory listing (VFS root):\n" );
		for( unsigned int i=0;i<dls.count();i++) {
			kprintf("* %u - %s\n", i, dls[i]->name);
		}

		dls.clear();
		fop_stat = vfs::list_directory( (unsigned char*)(const_cast<char*>("/cd")), &dls );
		if( fop_stat != vfs::vfs_status::ok ) {
			kprintf("FS list failed with error: %s\n", vfs::status_description(fop_stat));
		} else {
			kprintf("FS list succeeded.\n");
		}

		kprintf( "Directory listing (VFS, /cd):\n" );
		for( unsigned int i=0;i<dls.count();i++) {
			kprintf("* %u - %s\n", i, dls[i]->name);
		}

        logger_flush_buffer();
		while(true) { asm volatile("pause"); }

		*/

        /*
        f.write_file(test_file, (void*)test_file_data, strlen(test_file_data)+1);

		kprintf("File written out to %s\n", test_file_name);
		logger_flush_buffer();
		system_halt;
		*/

        /*
        test_file = f.create_file( (unsigned char*)(test_file_name), f.base );
		kprintf("test file created\n");
		logger_flush_buffer();
		system_halt;
		*/

        /*
        logger_flush_buffer();
		system_halt;

        if( test_file == NULL ) {
        	test_file = f.create_file( (unsigned char*)(test_file_name), f.base );
        	kprintf("test file created\n");
        	logger_flush_buffer();
			system_halt;
        }

        f.write_file(test_file, (void*)test_file_data, strlen(test_file_data)+1);

        kprintf("File written out to %s\n", test_file_name);
        logger_flush_buffer();
		system_halt;

        void *readback = kmalloc(strlen(test_file_data)+1);
        f.read_file(test_file, readback);

        char* readback_data = (char*)readback;
        kprintf("Readback data: %s\n", readback_data);
        logger_flush_buffer();
		system_halt;
		*/

        /*

        fat32_fs f2(1);
        root = f2.root_dir_vfs;
        kprintf( "Directory listing:\n" );
		for( unsigned int i=0;i<root->files.count();i++) {
			vfs_node *fn = root->files[i];
			kprintf( "* %u - %s\n", i, fn->name );
		}
        
		logger_flush_buffer();
		system_halt;
		*/

        // hacked-together raw disk function:
        /*
        while( true ) {
            unsigned int len = 0;
            kprintf("sector_n > ");
            char* sector_str = ps2_keyboard_readline(&len);
            if( len > 0 ) {
                uint32_t sector = atoi( sector_str );
                void *buf = kmalloc(512);
                io_read_partition( 1, buf, sector*512, 512 );
                uint8_t *buf2 = (uint8_t*)buf;
                for( int i=0;i<512;i+=16 ) {
                    kprintf("%#02x: ", i);
                    for( int j=0;j<16;j++ ) {
                        kprintf("%02x ", buf2[i+j]);
                    }
                    kprintf("\n");
                    set_message_listen_status( "keypress", true );
                    while(true) {
                        unique_ptr<ps2_keypress> kp;
                        kp = ps2_keyboard_get_keystroke();
                        if( !kp->released ) {
                            if( kp->key == KEY_Enter ) {
                                break;
                            }
                        }
                    }
                }
                kfree(buf);
            }
            kfree(sector_str);
        }
        */
        // hacked-together ls:
        /*
        kprintf("Reading partition 1 as FAT32...");
        fat32_fs *fs = new fat32_fs( 1 );
        kprintf("Reading root directory...\n");
        vfs_directory *root = fs->read_directory(0);
        for( int i=0;i<root->files.count();i++ ) {
            kprintf("* %u - %s\n", i, root->files[i]->name);
        }
        */
    }
}

void test_process_2() {
    volatile uint32_t loop_var = 0;
    while(true) {
        loop_var++;
        kprintf("Process 2! loop_var = %u.\n", (unsigned long long int)loop_var);
    }
}

extern "C" {
void kernel_main(multiboot_info_t* mb_info, unsigned int magic)
{
    char *test;
    int test2 = 0x12345678;
    page_frame* framez;
    page_frame* framez2;
    
    kprintf("Initializing multitasking.\n");
    process *proc1 = new process( (size_t)&test_process_1, false, 0, "init", NULL, 0 );
    //process *proc2 = new process( (size_t)&test_process_2, false, 0, "test_process_2", NULL, 0 );
    initialize_multitasking( proc1 );
    //spawn_process( proc2 );
        
    //terminal_writestring("Initializing serial interface.\n");
    //initialize_serial(COM1_BASE_PORT, 3);
    
    /*
    serial_print_basic("Serial port 1 active.\n");
    serial_write("Testing serial port behavior.\n");
    serial_write("Testing serial port behavior again\n");
    serial_write("Testing serial port behavior yet again\n");
    */
    
    terminal_writestring("Test: ");
    terminal_writestring(int_to_decimal(test2));
    kprintf("\nTest 2: %u / 0x%x\n.", test2, test2);
    
    atexit(&flush_serial_buffer, NULL);
    
    //block_for_interrupt(1);
    kprintf("Time since system startup: %u ticks.", get_sys_time_counter());
    
    kprintf("Testing kernel vmem allocation.\n");
    size_t vmem_test = k_vmem_alloc( 5 );
    kprintf("Virtual allocation starts at address 0x%x.\n", vmem_test);
    k_vmem_free( vmem_test );
    
    // Remap the VGA memory space.
    // This way, an errant terminal_writestring doesn't bring down the entire system.
    kprintf("Remapping VGA memory.\n");
    size_t vga_vmem = k_vmem_alloc( 1 );
    paging_set_pte( vga_vmem, 0xB8000, 0x101 );
    terminal_buffer = (uint16_t*)vga_vmem;
    kprintf("VGA buffer remapped to 0x%x.\n", (unsigned long long int)vga_vmem);
    
    kprintf("Initiating page fault!\n");
    int *pf_test = (int*)(0xC0F00004);
    *pf_test = 5;
    __sync_synchronize();
    kprintf("Memory read-back test: %u (should be 5)\n", (unsigned long long int)*(int*)(0xC0F00004));
    
    kprintf("Setup complete, starting processes!\n");
    multitasking_start_init();
    
    unsigned long long int last_ticked = 0;
    //timer t(1000, true, true, NULL);
    while(true) {
        //if(t.get_active()) {
        //    kprintf("Tock.");
        //    t.reload();
        //}
        if(last_ticked+1000 <= get_sys_time_counter()) {
            terminal_writestring("Tick.");
            last_ticked = get_sys_time_counter();
        }
    }
    //asm volatile("int $0" : : : "memory");
    //while(true)
    //    asm volatile( "hlt\n\t" : : : "memory");
}
}
