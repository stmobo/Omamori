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

#define AHCI_ERR_NO_SLOT 1
#define AHCI_ERR_TIMEOUT 2
#define AHCI_ERR_DISK    3

extern void ahci_initialize();
extern void ahci_stop_port( unsigned int );
extern void ahci_start_port( unsigned int );
extern unsigned int ahci_get_type( unsigned int );
extern unsigned int ahci_issue_data( unsigned int port, uint64_t lba_start, uint16_t sector_count, void* buffer, bool read );