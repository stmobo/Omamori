// irq.cpp - IRQ handlers

#include "includes.h"
#include "core/sys.h"
#include "arch/x86/pic.h"
#include "arch/x86/irq.h"
#include "lib/vector.h"

// remove these lines at some point (it's just for debugging)
#include "arch/x86/multitask.h"
#include "device/vga.h"

vector<size_t> irq_handlers[256];
signed int waiting_for = -1;
bool do_wait = false;
bool in_irq_context = false;

bool in_irq7 = false;
bool in_irq15 = false;

void do_irq(size_t irq_num, size_t eip, size_t cs) {
    /*
    if(irq_num == waiting_for)
        waiting_for = -1;
    */
    if( irq_num == 7 ) {
        if( in_irq7 || ((pic_get_isr() & 0x80) == 0) ) { // check to see if IRQ7 is actually happening (and don't enter the IRQ7 routine twice)
            return;
        } else {
            in_irq7 = true;
        }
    }
    if( irq_num == 15 ) {
        if( in_irq15 || ((pic_get_isr() & 0x8000) == 0) ) { // same thing as above, but with irq15
            return;
        } else {
            in_irq15 = true;
        }
    }
    if( irq_num == 255 ) {
    	return;
    }
    if( irq_num == 16 ) {
    	if( apics_initialized ) {
    		for(unsigned int i=0;i<8;i++) {
    			uint32_t isr = lapic_read_register( 0x100 + (0x10*i) );
    			if( isr != 0 ) {
    				for(unsigned int j=0;j<32;j++) {
    					if( (isr & (1<<j)) > 0 ) {
    						irq_num = (i*32)+j;
    						break;
    					}
    				}
    				break;
    			}
    		}
    	} else if( pic_8259_initialized ) {
			uint16_t isr = pic_get_isr();
			for(unsigned int i=0;i<16;i++) {
				if( isr & (1<<i) ) {
					irq_num = i;
					break;
				}
			}
    	}
    }
    
    in_irq_context = true;
    if(irq_num != 0) {
    	//irqsafe_kprintf("Handling irq: %u.\n", irq_num);
    	//logger_flush_buffer();
    	char* n = itoa( irq_num );
    	terminal_writestring( "Handling irq: " );
    	terminal_writestring( n );
    	terminal_writestring( "\n" );
    	kfree(n);
    }
	if(irq_handlers[irq_num].length() > 0) {
		for( unsigned int i=0;i<irq_handlers[irq_num].length();i++ ) {
			bool(*handler)(void) = (bool(*)())(irq_handlers[irq_num].get(i)); // jump to the stored function pointer...
			if( handler == NULL ) {
				irqsafe_kprintf("irq: NULL handler for irq %u?\n", irq_num);
				continue;
			}
			if(handler()) {
				break; // irq's handled, we're done here
			}
		}
	}

	irq_end_interrupt();

    if( irq_num == 7 ) {
        in_irq7 = false;
    }
    if( irq_num == 15 ) {
        in_irq15 = false;
    }
    in_irq_context = false;
    return;
}

bool irq_add_handler(int irq_num, size_t addr) {
    for( unsigned int i=0;i<irq_handlers[irq_num].length();i++ ) {
        if( irq_handlers[irq_num].get(i) == addr ) {
            return false;
        }
    }
    irq_handlers[irq_num].add(addr);
    irq_set_mask(irq_num, false);
    return true;
}

bool irq_remove_handler(int irq_num, size_t addr) {
    for( unsigned int i=0;i<irq_handlers[irq_num].length();i++ ) {
        if( irq_handlers[irq_num].get(i) == addr ) {
            irq_handlers[irq_num].remove(i);
        }
    }
    return true;
}

void block_for_irq(int irq) {
    if((irq < 0) || (irq > 15))
        return;
    waiting_for = irq;
    unsigned short mask = pic_get_mask();
    set_all_irq_status(true);
    irq_set_mask(irq, false);
    while(true) {
        system_wait_for_interrupt();
        if(waiting_for == -1)
            break;
    }
    pic_set_mask(mask);
}

void irq_set_mask(unsigned char irq, bool set) {
	if( apics_initialized ) {
		ioapic_redir_entry ent = apic_get_gsi_vector( irq );
		ent.masked = set;
		apic_set_gsi_vector( irq, ent );
	} else if( pic_8259_initialized ) {
		uint16_t mask = pic_get_mask();
		if( set ) {
			mask |= (1<<irq);
		} else {
			mask &= ~(1<<irq);
		}
		pic_set_mask(mask);
	}
}

bool irq_get_mask(unsigned char irq) {
	if( apics_initialized ) {
		ioapic_redir_entry ent = apic_get_gsi_vector( irq );
		return ent.masked;
	} else if( pic_8259_initialized ) {
		uint16_t mask = pic_get_mask();
		return ((mask & (1<<irq)) > 0);
	}
	return false;
}

void set_all_irq_status(bool status) {
	if( apics_initialized ) {
			for(unsigned int i=0;i<io_apics.count();i++) {
				io_apic *apic = io_apics[i];
				for(unsigned int j=0;j<apic->max_redir_entry;j++) {
					apic->entries[j].masked = status;
				}
				apic->update_redir_entries();
			}
	} else if( pic_8259_initialized ) {
		if(status) {
			pic_set_mask(0xFFFF);
		} else {
			pic_set_mask(0);
		}
	}
}

void irq_end_interrupt() {
	if( apics_initialized ) {
		lapic_eoi();
	} else if( pic_8259_initialized ) {
		pic_end_interrupt(irq_num);
	}
}
