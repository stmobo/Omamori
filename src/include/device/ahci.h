// ahci.h -- AHCI driver header

#pragma once

#define	SATA_SIG_SATA	0x00000101	// SATA drive
#define	SATA_SIG_SATAPI	0xEB140101	// SATAPI drive
#define	SATA_SIG_SEMB	0xC33C0101	// Enclosure management bridge
#define	SATA_SIG_PM 	0x96690101	// Port multiplier

#define SATA_DEV_NULL   0
#define SATA_DEV_SATA   1
#define SATA_DEV_SATAPI 2
#define SATA_DEV_SEMB   3
#define SATA_DEV_PM     4

	namespace ahci {
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
		uint32_t   cmd_list_upper;
		uint32_t   fis_base; // received FIS base
		uint32_t   fis_base_upper;
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

	void initialize();

	struct ahci_port {
		unsigned int port_number;

		page_frame* control_mem_phys; // Command List + Received FIS

		uintptr_t command_list_virt;
		uintptr_t received_fis_virt;

		volatile hba_mem* hba;
		volatile hba_port* registers;
	};
}
