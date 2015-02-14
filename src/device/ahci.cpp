// ahci.cpp - AHCI Driver

#include "includes.h"
#include "arch/x86/sys.h"
#include "arch/x86/irq.h"
#include "core/paging.h"
#include "core/scheduler.h"
#include "core/message.h"
#include "device/ahci.h"
#include "device/ata.h"
#include "device/pci.h"

using namespace ahci;

static vector<volatile hba_mem*> controllers;
static vector<ahci_port*> ports;

void ahci::initialize() {
	register_channel( "ahci_interrupt" );
    for(unsigned int i=0;i<pci_devices.count();i++) {
        pci_device *current = pci_devices[i];
        if( (current->class_code == 0x01) && (current->subclass_code == 0x06) && (current->prog_if == 0x01) ) {
            kprintf("ahci: found controller (device ID: %u [%u/%u/%u])\n", i, current->bus, current->device, current->func);

            for(unsigned int irq=0;irq<4;irq++) {
            	if( current->ints[irq] != 0 ) {
            		kprintf("ahci: installing handler for IRQ %u\n", current->ints[irq]);
            		irq_add_handler(current->ints[irq], &interrupt);
            	}
            }

            uintptr_t abar;
            if( current->registers[5].raw != 0 ) {
            	abar = current->registers[5].phys_address;
            	kprintf("ahci: base register found at: %#x\n", abar);
            } else {
            	page_frame* frames = pageframe_allocate(2);
            	pci_write_config_32( current->bus, current->device, current->func, 0x24, frames[0].address );
            	abar = frames[0].address;
            }
            uintptr_t vaddr = k_vmem_alloc(2);
            paging_set_pte( vaddr, abar, (1<<6) );
            paging_set_pte( vaddr+0x1000, abar+0x1000, (1<<6) );

            volatile hba_mem* hba = (hba_mem*)vaddr;

            hba->ghc |= (1<<31);
            hba->ghc |= (1<<1);

            kprintf("ahci: version %#x controller supports %u ports\n", hba->vs, (hba->cap & 0x1F)+1);
            for(unsigned int port_no=0;port_no<32;port_no++) {
            	if( (hba->pi&(1<<port_no)) > 0 ) {
            		ahci_port* p = new ahci_port;
            		p->hba = hba;
            		p->registers = &(hba->ports[port_no]);
            		p->port_number = port_no;

            		//p->stop_cmd();

            		p->control_mem_phys = pageframe_allocate(1);
            		p->registers->cmd_list = p->control_mem_phys->address;
            		p->registers->fis_base = p->control_mem_phys->address+0x400;

            		p->command_list_virt = k_vmem_alloc(1);

            		paging_set_pte( p->command_list_virt, p->control_mem_phys->address, (1<<6) );

            		p->received_fis_virt = p->command_list_virt+0x400;

            		p->received_fis = (volatile fis_received*)p->received_fis_virt;
            		p->cmd_list = (cmd_header*)p->command_list_virt;
            		p->c_tbl = (volatile cmd_table*)p->command_list_virt+0x400+sizeof(fis_received);

            		p->cmd_list[0].cmdt_addr = p->control_mem_phys->address+0x400+sizeof(fis_received);

            		kprintf("ahci: Port registers mapped to %#p\n", (void*)p->registers);
            		kprintf("ahci: Command list at %#x\n", p->command_list_virt);
            		kprintf("ahci: Received FIS at %#x\n", p->command_list_virt+0x400);
            		kprintf("ahci: Command table at %#x\n", p->command_list_virt+0x400+sizeof(fis_received));
            		logger_flush_buffer();

            		ports.add_end(p);

            		uint32_t stat = hba->ports[port_no].sata_stat;
            		uint8_t ipm = (stat >> 8) & 0x0F;
            		uint8_t det = stat & 0x0F;

            		// disable power state transitions
            		p->registers->sata_ctrl |= 0x300;

            		// clear int state
            		p->registers->int_status = p->registers->int_status;

            		// enable all interrupts
					p->registers->int_enable = 0xFF;

					// clear error bits
					p->registers->sata_error = p->registers->sata_error;

					p->registers->cmd_stat |= (1<<2); // power on device

					p->registers->cmd_stat |= (1<<1); // spin up device

					p->registers->cmd_stat = (p->registers->cmd_stat & ~(0xF<<28)) | (1<<28); // enable link

					p->registers->cmd_stat |= (1<<4); // enable FIS Receive DMA

					p->registers->cmd_stat |= 1; 	 // start command list processing

            		if( det == 3 ) {
            			if( ipm == 1 ) {
							switch( hba->ports[port_no].sig ) {
							case device_types::sata_drive:
								kprintf("ahci: found SATA drive on port %u\n", port_no);
								p->type = device_types::sata_drive;
								break;
							case device_types::satapi_drive:
								kprintf("ahci: found SATAPI drive on port %u\n", port_no);
								p->type = device_types::satapi_drive;
								break;
							case device_types::semb_drive:
								kprintf("ahci: found SEMB drive on port %u\n", port_no);
								p->type = device_types::semb_drive;
								break;
							case device_types::port_multiplier:
								kprintf("ahci: found Port Multiplier drive on port %u\n", port_no);
								p->type = device_types::port_multiplier;
								break;
							default:
								kprintf("ahci: found unknown drive on port %u\n", port_no);
								p->type = device_types::none;
								break;
							}
            			} else {
            				kprintf("ahci: found sleeping drive on port %u\n", port_no);
            				p->type = device_types::none;
            			}
            		} else {
            			kprintf("ahci: found no drive on port %u\n", port_no);
            			p->type = device_types::none;
            		}
            	}
            }
        }
    }

    for(unsigned int i=0;i<ports.count();i++) {
    	page_frame *ident_frame = pageframe_allocate(1);
    	size_t ident_vmem = k_vmem_alloc(1);

    	paging_set_pte(ident_vmem, ident_frame->address, (1<<6));
    	kprintf("ahci: ident data at %#x\n", ident_vmem);
    	logger_flush_buffer();

    	ports[i]->identify(ident_frame);

    	kprintf("ahci: ident data received\n");
    	logger_flush_buffer();
    	system_halt;
    }
}

void ahci::ahci_port::stop_cmd() {
	this->registers->cmd_stat &= ~1; // disable command list processing

	while( (this->registers->cmd_stat & ((1<<15) | (1<<14))) > 0 ) asm volatile("pause"); // wait for PxCMD.CR and PxCMD.FR (FIS Receive) to clear

	this->registers->cmd_stat &= ~(1<<4); // disable FIS Receive DMA
}

void ahci::ahci_port::start_cmd() {
	while( (this->registers->cmd_stat & (1<<15)) > 0 ) asm volatile("pause"); // wait for PxCMD.CR (Command list Running) to clear

	this->registers->cmd_stat |= (1<<4); // enable FIS Receive DMA
	this->registers->cmd_stat |= 1; 	 // start command list processing
}

bool ahci::interrupt( uint8_t irq_num ) {
	for(unsigned int i=0;i<controllers.count();i++) {
		for(unsigned int port=0;port<32;port++) {
			if( ((controllers[i]->pi & (1<<port)) > 0) && ((controllers[i]->is & (1<<port)) > 0) ) {
				message wakeup_call(NULL, port);

				irqsafe_kprintf("ahci: irq received on line %u\n", irq_num);

				send_to_channel( "ahci_interrupt", wakeup_call );

				return true; // yep, this port signaled an interrupt
			}
		}
	}

	return false;
}

int ahci::ahci_port::find_cmd_slot() {
	unsigned int ncs = ((this->hba->cap >> 8) & 0x1F);
	for(unsigned int slot=0;slot<=ncs;slot++) {
		if( ((this->registers->sata_active & (1<<slot)) == 0) && ((this->registers->cmd_issue & (1<<slot)) == 0) ) {
			return slot;
		}
	}
	return -1;
}

bool ahci::ahci_port::identify( page_frame *dest_frame ) {
	fis_reg_h2d* fis = (fis_reg_h2d*)(this->c_tbl->cmd_fis);
	logger_flush_buffer();

	fis->dev_sel = 0;

	fis->fis_type = fis_type::reg_h2d;
	fis->cmd = ATA_CMD_IDENTIFY;
	fis->pmport_c = 0x80;
	fis->count_lo = 1;

	this->c_tbl->prdt[0].count = 512 | (1<<31);
	this->c_tbl->prdt[0].dba = dest_frame->address;

	this->cmd_list->prdt_length = 1;
	this->cmd_list->params = 4;

	channel_receiver ahci_int = listen_to_channel("ahci_interrupt");

	// set CI
	this->registers->cmd_issue = 1;

	//then wait for TFD.BSY and CI to clear
	while(true) {
		//ahci_int.wait();

		//message *m = ahci_int.queue.remove(0);

		//unsigned int port = m->data_size;
		//delete m;

		if( (this->registers->task_fdata & ATA_SR_BSY) > 0) {
			kprintf("ahci: drive has status BSY\n");
		}

		if( (this->registers->task_fdata & ATA_SR_DRQ) > 0) {
			kprintf("ahci: drive has status DRQ\n");
		}

		if( (this->registers->task_fdata & ATA_SR_ERR) > 0) {
			kprintf("ahci: drive has status ERR\n");
		}
		logger_flush_buffer();

		//if( port == this->port_number ) {
			if( ((this->registers->task_fdata & ATA_SR_BSY) > 0) || ((this->registers->cmd_issue & 1) > 0) ) {
				asm volatile("pause");
			} else {
				// command's complete
				break;
			}
		//}
	}

	return true;
}

bool ahci::ahci_port::send_rw_command( bool write, uint64_t start, unsigned int count, page_frame* dest_pages ) {
	return false;
}


ahci::request::request() {
	this->frame = pageframe_allocate(1);
	this->vaddr = k_vmem_alloc(1);
	this->tbl = (volatile cmd_table*)this->vaddr;

	paging_set_pte( this->vaddr, this->frame->address, (1<<6) );
}

ahci::request::~request() {
	pageframe_deallocate(this->frame, 1);
	k_vmem_free( this->vaddr );
}

ahci::request::request( request& rhs ) {
	this->frame = pageframe_allocate(1);
	this->vaddr = k_vmem_alloc(1);
	this->tbl = (volatile cmd_table*)this->vaddr;

	paging_set_pte( this->vaddr, this->frame->address, (1<<6) );

	memcpy( (void*)this->vaddr, (void*)rhs.vaddr, 0x1000 );
}

request& ahci::request::operator=( request& rhs ) {
	this->frame = pageframe_allocate(1);
	this->vaddr = k_vmem_alloc(1);
	this->tbl = (volatile cmd_table*)this->vaddr;

	paging_set_pte( this->vaddr, this->frame->address, (1<<6) );

	memcpy( (void*)this->vaddr, (void*)rhs.vaddr, 0x1000 );

	return *this;
}
