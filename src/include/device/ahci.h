// ahci.h -- AHCI driver header

#pragma once

extern void ahci_initialize();
extern void ahci_stop_port( unsigned int );
extern void ahci_start_port( unsigned int );