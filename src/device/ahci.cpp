// ahci.cpp - AHCI Driver

#include "includes.h"
#include "arch/x86/sys.h"
#include "core/paging.h"
#include "core/scheduler.h"
#include "core/message.h"
#include "device/ahci.h"
#include "device/ata.h"
#include "device/pci.h"

using namespace ahci;

static vector<volatile hba_mem*> controllers;
static vector<volatile ahci_port*> ports;

void ahci::initialize() {
    //register_channel( "ahci_transfer_complete" );
    for(unsigned int i=0;i<pci_devices.count();i++) {
        pci_device *current = pci_devices[i];
        if( (current->class_code == 0x01) && (current->subclass_code == 0x06) && (current->prog_if == 0x01) ) {
            kprintf("ahci: found controller (device ID: %u [%u/%u/%u])\n", i, current->bus, current->device, current->func);
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
            kprintf("ahci: version %#x controller supports %u ports\n", hba->vs, (hba->cap & 0x1F)+1);
            for(unsigned int port_no=0;port_no<32;port_no++) {
            	if( (hba->pi&(1<<port_no)) > 0 ) {
            		volatile ahci_port* p = new ahci_port;
            		p->hba = hba;
            		p->registers = &(hba->ports[port_no]);
            		p->port_number = port_no;

            		p->control_mem_phys = pageframe_allocate(1);
            		p->registers->cmd_list = p->control_mem_phys->address;
            		p->registers->fis_base = p->control_mem_phys->address+0x400;

            		p->command_list_virt = k_vmem_alloc(1);
            		p->received_fis_virt = p->command_list_virt+0x400;

            		paging_set_pte( p->command_list_virt, p->control_mem_phys->address, (1<<6) );

            		ports.add_end(p);

            		uint32_t stat = hba->ports[port_no].sata_stat;
            		uint8_t ipm = (stat >> 8) & 0x0F;
            		uint8_t det = stat & 0x0F;

            		if( det == 3 ) {
            			if( ipm == 1 ) {
							switch( hba->ports[port_no].sig ) {
							case SATA_SIG_SATA:
								kprintf("ahci: found SATA drive on port %u\n", port_no);
								break;
							case SATA_SIG_SATAPI:
								kprintf("ahci: found SATAPI drive on port %u\n", port_no);
								break;
							case SATA_SIG_SEMB:
								kprintf("ahci: found SEMB drive on port %u\n", port_no);
								break;
							case SATA_SIG_PM:
								kprintf("ahci: found Port Multiplier drive on port %u\n", port_no);
								break;
							default:
								kprintf("ahci: found unknown drive on port %u\n", port_no);
								break;
							}
            			} else {
            				kprintf("ahci: found sleeping drive on port %u\n", port_no);
            			}
            		} else {
            			kprintf("ahci: found no drive on port %u\n", port_no);
            		}
            	}
            }
        }
    }
}
