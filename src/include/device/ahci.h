// ahci.h -- AHCI driver header

#pragma once

namespace ahci {
	enum device_types {
		sata_drive = 0x00000101,
		satapi_drive = 0xEB140101,
		semb_drive = 0xC33C0101,
		port_multiplier = 0x96690101,
		none = 0,
	};

	typedef enum fis_type {
		reg_h2d      = 0x27,	// Register FIS - host to device
		reg_d2h	     = 0x34,	// Register FIS - device to host
		activate_dma = 0x39,	// DMA activate FIS - device to host
		setup_dma	 = 0x41,	// DMA setup FIS - bidirectional
		data		 = 0x46,	// Data FIS - bidirectional
		bist		 = 0x58,	// BIST activate FIS - bidirectional
		setup_pio	 = 0x5F,	// PIO setup FIS - device to host
		device_bits	 = 0xA1,	// Set device bits FIS - device to host
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
		uint32_t cmd_list; 		  // PxCLB
		uint32_t cmd_list_upper;  // PxCLBU
		uint32_t fis_base; 		  // PxFB
		uint32_t fis_base_upper;  // PxFBU
		uint32_t int_status;	  // PxIS
		uint32_t int_enable;	  // PxIE
		uint32_t cmd_stat;		  // PxCMD
		uint32_t rsvd0 = 0;		  // reserved
		uint32_t task_fdata;	  // PxTFD
		uint32_t sig;			  // PxSIG
		uint32_t sata_stat;		  // PxSSTS
		uint32_t sata_ctrl;		  // PxSCTL
		uint32_t sata_error;	  // PxSERR
		uint32_t sata_active;	  // PxSACT
		uint32_t cmd_issue;		  // PxCI
		uint32_t stat_notify;	  // PxSNTF
		uint32_t fis_switch_ctrl; // PxFBS
		uint32_t rsvd1[11];		  // reserved
		uint32_t vendor[4];		  // PxVS
	} __attribute__((packed));

	struct hba_mem {
		uint32_t cap;		// 0x00
		uint32_t ghc;		// 0x04
		uint32_t is;		// 0x08
		uint32_t pi;		// 0x0C
		uint32_t vs;	 	// 0x10
		uint32_t ccc_ctrl;  // 0x14
		uint32_t ccc_pts;	// 0x18
		uint32_t em_loc;	// 0x1C
		uint32_t em_ctl;	// 0x20
		uint32_t cap_ext;	// 0x24
		uint32_t bohc;		// 0x28

		uint8_t rsvd[0xA0-0x2C];	// 0x74 bytes
		uint8_t vendor[0x100-0xA0]; // 0x60 bytes

		volatile hba_port ports[32];
	} __attribute__((packed));

	struct fis_received {
		fis_dma_setup dma_setup_fis;
		uint8_t 	  pad0[0x20-0x1C];

		fis_pio_setup pio_setup_fis;
		uint8_t 	  pad1[0x40-0x34];

		fis_reg_d2h   d2h_reg_fis;
		uint8_t 	  pad2[0x58-0x54];

		fis_dev_bits  dev_bits_fis;

		uint8_t       ufis[64];
	} __attribute__((packed));

	struct cmd_header {
		uint16_t params;
		// 0:4 - Command FIS length in Dwords, 2 - 16
		// 5 - ATAPI bit
		// 6 - Direction (1 - Host->Device, 0 - Device->Host)
		// 7 - Prefetchable
		// 8 - Reset
		// 9 - BIST
		// 10 - Clear busy upon R_OK
		// 11 - Reserved
		// 12:15 - Port multiplier port

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

		prdt_entry prdt[1];
	} __attribute__((packed)); // for ATAPI commands / devices: issue ATAPI_PACKET command, set atapi_cmd (above), and set the ATAPI bit in the cmd_header structure.

	struct request {
		page_frame *frame;
		uintptr_t vaddr;

		volatile cmd_table* tbl;

		request();
		~request();

		request& operator=(request& rhs);
		request(request&);
	};

	struct ahci_port {
		unsigned int port_number;

		page_frame* control_mem_phys; // Command List + Received FIS

		uintptr_t command_list_virt;
		uintptr_t received_fis_virt;

		device_types type;

		volatile hba_mem* hba;
		volatile hba_port* registers;
		volatile fis_received* received_fis;
		volatile cmd_table* c_tbl;
		cmd_header* cmd_list;

		void start_cmd();
		void stop_cmd();
		int find_cmd_slot();
		bool identify( page_frame* dest_frame );
		bool send_rw_command( bool write, uint64_t start, unsigned int count, page_frame* dest_pages );
	};

	void initialize();
	bool interrupt( uint8_t irq_num );
}
