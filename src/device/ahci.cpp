// ahci.cpp - AHCI Driver

#include "includes.h"
#include "arch/x86/sys.h"
#include "core/paging.h"
#include "core/scheduler.h"
#include "core/message.h"
#include "device/ahci.h"
#include "device/ata.h"
#include "device/pci.h"

static uint32_t abar;

static uint64_t recv_fis_virt[32] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
static uint64_t recv_fis_phys[32] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

static uint64_t cmd_list_virt[32] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
static uint64_t cmd_list_phys[32] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

static uint8_t  n_cmd_list;
static uint8_t  n_ports;

typedef enum fis_type {
	FIS_TYPE_REG_H2D	= 0x27,	// Register FIS - host to device
	FIS_TYPE_REG_D2H	= 0x34,	// Register FIS - device to host
	FIS_TYPE_DMA_ACT	= 0x39,	// DMA activate FIS - device to host
	FIS_TYPE_DMA_SETUP	= 0x41,	// DMA setup FIS - bidirectional
	FIS_TYPE_DATA		= 0x46,	// Data FIS - bidirectional
	FIS_TYPE_BIST		= 0x58,	// BIST activate FIS - bidirectional
	FIS_TYPE_PIO_SETUP	= 0x5F,	// PIO setup FIS - device to host
	FIS_TYPE_DEV_BITS	= 0xA1,	// Set device bits FIS - device to host
} fis_type;

struct fis_reg_h2d {
    uint8_t fis_type;
    uint8_t pmport_c; // first 4 bits are the port multiplier, and the last bit denotes command or control
    uint8_t cmd;      // bit seven of pmport_c determines whether this writes to the command or control registers
    uint8_t feature_lo;
    
    uint8_t lba_0;
    uint8_t lba_1;
    uint8_t lba_2;
    uint8_t dev_sel;
    
    uint8_t lba_3;
    uint8_t lba_4;
    uint8_t lba_5;
    uint8_t feature_hi;
    
    uint8_t count_lo;
    uint8_t count_hi;
    uint8_t icc;
    uint8_t ctl;
    
    uint32_t rsvd  = 0;
} __attribute__((packed));

struct fis_reg_d2h {
    uint8_t fis_type;
    uint8_t pmport_i; // 1st 4 bits are the port multiplier, and bit 6 is the interrupt bit
    uint8_t status;
    uint8_t error;
    
    uint8_t lba_0;
    uint8_t lba_1;
    uint8_t lba_2;
    uint8_t dev_sel;
    
    uint8_t lba_3;
    uint8_t lba_4;
    uint8_t lba_5;
    uint8_t rsvd0 = 0;
    
    uint8_t count_lo;
    uint8_t count_hi;
    uint8_t rsvd1 = 0;
    uint8_t rsvd2 = 0;
    
    uint32_t rsvd3 = 0;
} __attribute__((packed));

struct fis_data {
    uint8_t fis_type;
    uint8_t pmport; // last 4 bits are reserved
    uint16_t rsvd = 0;
    
    uint32_t payload[1];
} __attribute__((packed));

struct fis_pio_setup {
    uint8_t fis_type;
    uint8_t pmport; // 0:3 - port mul, 5 - set to 1 for reads, 6 - interrupt bit
    uint8_t status;
    uint8_t error;
    
    uint8_t lba_0;
    uint8_t lba_1;
    uint8_t lba_2;
    uint8_t dev_sel;
    
    uint8_t lba_3;
    uint8_t lba_4;
    uint8_t lba_5;
    uint8_t rsvd0 = 0;
    
    uint8_t count_lo;
    uint8_t count_hi;
    uint8_t rsvd1 = 0;
    uint8_t new_status;
    
    uint16_t byte_transfer_count;
    uint16_t rsvd2  = 0;
} __attribute__((packed));

struct fis_dma_setup {
    uint8_t fis_type;
    uint8_t pmport; // 0:3 - port mul, 5 - set to 1 for reads, 6 - interrupt bit, 7 - Auto-activate
    uint16_t rsvd0 = 0;
    
    uint64_t buffer_id;
    
    uint32_t rsvd1 = 0;
    
    uint32_t buffer_offset; // first two bits must be zero
    
    uint32_t byte_transfer_count; // first bit must be 0
    
    uint32_t rsvd2 = 0; 
} __attribute__((packed));

struct fis_dev_bits {
    uint8_t  fis_type;
    uint8_t  interrupt; // bit 6 - interrupt (all others reserved)
    uint8_t  status;    // 0:2 and 4:6 - Status Hi/Lo, all others reserved
    uint8_t  error;
    
    uint32_t s_active;
} __attribute__((packed));

struct hba_port {
    uint32_t   cmd_list; // command list base
    uint32_t   cmd_list_u;
    uint32_t   fis_base; // received FIS base
    uint32_t   fis_base_u;
    //uint32_t cmd_list;
    //uint32_t cmd_list_upper;
    //uint32_t fis_base;
    //uint32_t fis_base_upper;
    uint32_t int_status;
    uint32_t int_enable;
    uint32_t cmd_stat;
    uint32_t rsvd0 = 0;
    uint32_t task_fdata;
    uint32_t sig;
    uint32_t sata_stat;
    uint32_t sata_ctrl;
    uint32_t sata_error;
    uint32_t sata_active;
    uint32_t cmd_issue;
    uint32_t stat_notify;
    uint32_t fis_switch_ctrl;
    uint32_t rsvd1[11];
    uint32_t vendor[4];
} __attribute__((packed));

struct hba_mem {
    uint32_t cap;
    uint32_t ghc;
    uint32_t is;
    uint32_t pi;
    uint32_t vs;
    uint32_t ccc_ctrl;
    uint32_t ccc_pts;
    uint32_t em_loc;
    uint32_t em_ctl;
    uint32_t cap_ext;
    uint32_t bohc;
    
    uint8_t rsvd[0xA0-0x2C];
    uint8_t vendor[0x100-0xA0];
    
    volatile hba_port ports[32];
} __attribute__((packed));

struct fis_received {
    fis_dma_setup dma_setup_fis;
    uint32_t      pad0;
    
    fis_pio_setup pio_setup_fis;
    uint32_t      pad1;
    
    fis_reg_d2h   d2h_reg_fis;
    uint32_t      pad2;
    
    fis_dev_bits  dev_bits_fis;
    
    uint8_t       ufis[64];
    uint8_t       rsvd[0x100 - 0xA0];
} __attribute__((packed));

struct cmd_header {
    uint8_t lo_params;
    // 0:4 - Command FIS length in Dwords, 2 - 16
    // 5 - ATAPI bit
    // 6 - Direction (1 - Host->Device, 0 - Device->Host)
    // 7 - Prefetchable
    
    uint8_t hi_params;
    // 0 - Reset
    // 1 - BIST
    // 2 - Clear busy upon R_OK
    // 3 - Reserved
    // 4:7 - Port multiplier port
    
    uint16_t prdt_length;
    
    volatile uint32_t prd_bytes_transferred;
    
    uint64_t cmdt_addr;
    
    uint32_t rsvd[4];
} __attribute__((packed));

struct prdt_entry {
    uint32_t dba; // bit zero must be zero
    uint32_t dba_u;
    uint32_t rsvd = 0;
    uint32_t count; // bits 22:30 are reserved, bit 31 indicates whether to fire an interrupt after completion.
} __attribute__((packed));

struct cmd_table {
    uint8_t cmd_fis[64];
    
    uint8_t atapi_cmd[16];
    
    uint8_t rsvd[48];
    
    volatile prdt_entry prdt[1];
} __attribute__((packed)); // for ATAPI commands / devices: issue ATAPI_PACKET command, set atapi_cmd (above), and set the ATAPI bit in the cmd_header structure. 

volatile static hba_mem *hba;

#define is_port_active(i) ((hba->pi&(1<<i)) > 0)

void ahci_initialize() {
    register_channel( "ahci_transfer_complete" );
    for(unsigned int i_dont_even=0;i_dont_even<pci_devices.count();i_dont_even++) {
        pci_device *current = pci_devices[i_dont_even];
        if( (current->class_code == 0x01) && (current->subclass_code == 0x06) ) {
            kprintf("ahci: found controller (device ID: %u [%u/%u/%u])\n", i_dont_even, current->bus, current->device, current->func);
            
            size_t tmp_vaddr = k_vmem_alloc(2);
            
            // fill in the default values for the other BARs: 0x1F0, 0x3F6, 0x170, 0x376
            //pci_write_config_32( current->bus, current->device, current->func, 0x10, 0x1F0 );
            //pci_write_config_32( current->bus, current->device, current->func, 0x14, 0x3F6 );
            //pci_write_config_32( current->bus, current->device, current->func, 0x18, 0x170 );
            //pci_write_config_32( current->bus, current->device, current->func, 0x1C, 0x376 );
            //pci_write_config_32( current->bus, current->device, current->func, 0x20, ATA_BUS_MASTER_START );
            // set HBA thing
            uint32_t whaaat = pci_read_config_32( current->bus, current->device, current->func, 0x24 );
            if( whaaat != 0 ) {
                kprintf("ahci: HBA already located at 0x%x.\n", whaaat);
                abar = whaaat;
            } else {
                page_frame *hba_frames = pageframe_allocate(2);
                abar = hba_frames[0].address;
                pci_write_config_32( current->bus, current->device, current->func, 0x24, abar<<13 );
            }
            paging_set_pte( tmp_vaddr, abar, 0x81 );
            paging_set_pte( tmp_vaddr+0x1000, abar+0x1000, 0x81 ); // set Cache Disable
            hba = (volatile hba_mem*)tmp_vaddr;
            // PCI command register:
            //pci_write_config_16( current->bus, current->device, current->func, 0x04, 0x6 ); // Bus Master Enable | Memory Space Enable
            for(unsigned int i=0;i<32;i++) {
                if( is_port_active(i) ) {
                    n_ports++;
                    ahci_stop_port( i );
                }
            }
            
            if( n_ports <= 29 ) {
                pageframe_allocate_at( abar, 1 );
            } else {
                pageframe_allocate_at( abar, 2 );
            }
            
            hba->ghc |= 0x80000000; // set AE
            
            n_cmd_list = ((hba->cap&0x1F00)>>8) & 0x1F; 
            unsigned int n_fis_pages = ((n_ports+(0x10-(n_ports%0x10))) / 0x10)+1; // each RFIS is 256 bytes long, so 16 RFISes take up a page.
            // so we only need 2 pages for all 32 ports.
            
            for(unsigned int i=0;i<n_ports;i++) {
                cmd_list_virt[i] = k_vmem_alloc(1);
                if( hba->ports[i].cmd_list != 0 ) {
                    pageframe_allocate_at( hba->ports[i].cmd_list, 1 );
                    cmd_list_phys[i] = hba->ports[i].cmd_list;
                } else {
                    page_frame *frame = pageframe_allocate(1);
                    cmd_list_phys[i] = frame->address;
                    delete frame;
                }
                paging_set_pte( cmd_list_virt[i], cmd_list_phys[i], 0x81 );
                kprintf("ahci: port %u: command list at 0x%x.\n", i, cmd_list_phys[i] );
            }
            
            size_t      fis_vaddr  = k_vmem_alloc( (n_ports / 4)+1 );
            page_frame* fis_frames = pageframe_allocate( (n_ports / 4)+1 );
            
            size_t fis_current_v = fis_vaddr;
            size_t fis_current_p = fis_frames->address;
            
            for( int i=0;i<=(n_ports / 4);i++ ) {
                paging_set_pte( fis_vaddr+(i*0x1000), fis_frames[i].address, 0x81 );
            }
            
            for(unsigned int i=0;i<n_ports;i++) { // note to self: find a more efficient way to map this
                recv_fis_phys[i] = fis_current_p;
                recv_fis_virt[i] = fis_current_v;
                
                fis_current_v += 256;
                fis_current_p += 256;
            }
            
            kprintf("ahci: controller capability flags are 0x%x.\n", hba->cap );
            kprintf("ahci: controller supports %u ports with %u command entries per list.\n", n_ports, n_cmd_list );
            
            for(unsigned int i=0;i<n_ports;i++) {
                hba->ports[i].fis_base = recv_fis_phys[i];
                hba->ports[i].fis_base_u = 0;
                
                hba->ports[i].cmd_list = cmd_list_phys[i];
                hba->ports[i].cmd_list_u = 0;
                hba->ports[i].sata_error = 0xFFFFFFFF;
                
                while( (hba->ports[i].cmd_stat & 0x8000) > 0 );
        
                hba->ports[i].cmd_stat |= 0x10; // set FIS Recv. Enable
                hba->ports[i].int_enable = 0x7FC0007F;
                
                switch( ahci_get_type( i ) ) {
                    case SATA_DEV_SATA:
                        kprintf("ahci: device %u is a SATA drive.\n", i);
                        break;
                    case SATA_DEV_SATAPI:
                        kprintf("ahci: device %u is a SATAPI device.\n", i);
                        break;
                    case SATA_DEV_SEMB:
                        kprintf("ahci: device %u is an enclosure management bridge.\n", i);
                        break;
                    case SATA_DEV_PM:
                        kprintf("ahci: device %u is a port multiplier.\n", i);
                        break;
                    case SATA_DEV_NULL:
                        kprintf("ahci: device %u is not present.\n", i);
                        break;
                    default:
                        kprintf("ahci: device %u is unknown.\n", i);
                        break;
                }
            }
            
            flushCache();
            
            hba->ghc |= 2; // Interrupt Enable
            
            flushCache();
            return;
        }
    }
}

void ahci_stop_port( unsigned int port ) {
    if( is_port_active(port) ) {
        hba->ports[port].cmd_stat &= ~0x01; // clear Start
        
        while( (hba->ports[port].cmd_stat & 0xC000) > 0 );
        
        hba->ports[port].cmd_stat &= ~0x10; // clear FIS Recv. Enable
    }
}

void ahci_start_port( unsigned int port ) {
    if( is_port_active(port) ) {
        while( (hba->ports[port].cmd_stat & 0x8000) > 0 );
        
        hba->ports[port].cmd_stat |= 0x10; // set FIS Recv. Enable
        hba->ports[port].cmd_stat |= 0x01; // set Start
    }
}

unsigned int ahci_get_type( unsigned int port ) {
    uint32_t ssts = hba->ports[port].sata_stat;
    uint8_t  det  = ssts & 0x0F;
    uint8_t  ipm  = (ssts>>8) & 0x0F;
    if( (det == 3) && (ipm == 1) ) {
        switch( hba->ports[port].sig ) {
            case SATA_SIG_SATA:
                return SATA_DEV_SATA;
            case SATA_SIG_SATAPI:
                return SATA_DEV_SATAPI;
            case SATA_SIG_SEMB:
                return SATA_DEV_SEMB;
            case SATA_SIG_PM:
                return SATA_DEV_PM;
            default:
                return SATA_DEV_SATA;
        }
    } else {
        return SATA_DEV_NULL;
    }
}

int ahci_find_command_slot( unsigned int port ) {
    uint32_t slot_status = ( hba->ports[port].sata_active | hba->ports[port].cmd_issue );
    for(unsigned int i=0;i<n_cmd_list;i++) {
        if( (slot_status & 1) == 0 ) { // check if bit is not set in either SACT or CI.
            return i;
        } else
            slot_status >>= 1;
    }
    return -1;
}

// Issue a data command
unsigned int ahci_issue_data( unsigned int port, uint64_t lba_start, uint16_t sector_count, void* buffer, bool read ) {
    kprintf("ahci: beginning AHCI transaction.\n");
    hba->ports[port].int_status = 0xFFFFFFFF; // clear Interrupt Status
    int cmd_slot = ahci_find_command_slot( port );
    if( cmd_slot == -1 ) {
        kprintf("ahci: could not complete request: no free command slots found for port %u.\n", port);
        return AHCI_ERR_NO_SLOT;
    }
    
    kprintf("ahci: using command slot %u.\n", cmd_slot);
    
    cmd_header *hdr = (cmd_header*)(cmd_list_virt[port]);
    hdr += cmd_slot;
    hdr->lo_params = (sizeof(fis_reg_h2d) / 4) | ( read ? 0 : (1<<6) );
    hdr->hi_params = 0;
    hdr->prdt_length = ((sector_count-1)>>4)+1;
    kprintf("ahci: prdt length is %u.\n", hdr->prdt_length);
    
    // okay, maybe find a better / more efficient way to reserve physical memory?
    unsigned int n_pages = ((sizeof(cmd_table) + ((hdr->prdt_length-1)*sizeof(prdt_entry))) / 0x1000)+1;
    size_t tbl_vmem = k_vmem_alloc(n_pages);
    if( hdr->cmdt_addr == NULL ) {
        page_frame *tbl_frames = pageframe_allocate(n_pages);
        logger_flush_buffer();
        hdr->cmdt_addr = tbl_frames->address;
    }
    for(int i=0;i<n_pages;i++) {
        paging_set_pte( tbl_vmem+(i*0x1000), hdr->cmdt_addr+(i*0x1000), 0x81 );
    }
    cmd_table *tbl = (cmd_table*)(tbl_vmem);
    memclr( (void*)tbl, sizeof(cmd_table) + ((hdr->prdt_length-1)*sizeof(prdt_entry)) );
    uint32_t buf_tmp = (uint32_t)buffer;
    uint32_t cnt_tmp = (uint32_t)sector_count;
    
    for(unsigned int i=0;i<hdr->prdt_length-1;i++) {
        tbl->prdt[i].dba   = buf_tmp;
        tbl->prdt[i].dba_u = 0;
        tbl->prdt[i].count = 0x2000 | 0x80000000;
        buf_tmp += 0x2000;
        cnt_tmp -= 16;
    }
    tbl->prdt[ hdr->prdt_length-1 ].dba   = buf_tmp;
    tbl->prdt[ hdr->prdt_length-1 ].dba_u = 0;
    tbl->prdt[ hdr->prdt_length-1 ].count = (cnt_tmp<<9) | 0x80000000;
    
    fis_reg_h2d *cmd_fis = (fis_reg_h2d*)(tbl->cmd_fis);
    cmd_fis->fis_type = FIS_TYPE_REG_H2D;
    cmd_fis->pmport_c = 0x80;
    cmd_fis->cmd = ( read ? ATA_CMD_READ_DMA_EXT : ATA_CMD_WRITE_DMA_EXT );
    
    cmd_fis->lba_0 = (uint8_t)(lba_start & 0xFF);
    cmd_fis->lba_1 = (uint8_t)((lba_start>>8) & 0xFF);
    cmd_fis->lba_2 = (uint8_t)((lba_start>>16) & 0xFF);
    cmd_fis->dev_sel = (1<<6);
    
    cmd_fis->lba_3 = (uint8_t)((lba_start>>24) & 0xFF);
    cmd_fis->lba_4 = (uint8_t)((lba_start>>32) & 0xFF);
    cmd_fis->lba_5 = (uint8_t)((lba_start>>40) & 0xFF);
    
    cmd_fis->count_lo = (uint8_t)(sector_count&0xFF);
    cmd_fis->count_hi = (uint8_t)((sector_count>>8)&0xFF);
    
    flushCache();
    
    uint64_t start_time = get_sys_time_counter();
    while( ( hba->ports[port].task_fdata & (ATA_SR_BSY | ATA_SR_DRQ) ) && (start_time+5000 <= get_sys_time_counter()) );
    if( (start_time+5000 <= get_sys_time_counter()) ) {
        kprintf("ahci: could not complete request: port %u is hung.\n", port);
        return AHCI_ERR_TIMEOUT;
    }
    
    kprintf("ahci: sending command.\n");
    hba->ports[port].cmd_stat |= 1; // Start Command Engine
    
    hba->ports[port].sata_active = (1<<cmd_slot);
    hba->ports[port].cmd_issue   = (1<<cmd_slot);
    
    flushCache();
    
    start_time = get_sys_time_counter();
    while( true ) {
        if( ((hba->ports[port].cmd_issue) & (1<<cmd_slot)) == 0 ) {
            break;
        }
    
        if( ( hba->ports[port].int_status & (1<<30) ) > 0 ) {
            kprintf("ahci: could not complete request: disk read error on port %u.\n", port);
            return AHCI_ERR_DISK;
        }
    }
    
    hba->ports[port].cmd_stat &= ~1; // Stop Command Engine
    return 0;
}
