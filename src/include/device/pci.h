// pci.h
#pragma once
#include "includes.h"
#include "lib/vector.h"

#define PCI_IO_CONFIG_ADDRESS       0xCF8
#define PCI_IO_CONFIG_DATA          0xCFC

typedef struct pcie_ecs_range {
    uint32_t mmio_paddr;
    char     bus_start;
    char     bus_end;
    size_t   vaddr;
} pcie_ecs_range;

typedef struct pci_device {
    uint8_t bus;
    uint8_t device;
    uint8_t func;
    uint16_t vendorID;
    uint16_t deviceID;
    uint8_t class_code;
    uint8_t subclass_code;
    uint8_t prog_if;
    uint8_t header_type;
} pci_device;

extern vector<pci_device*> pci_devices;

extern void pci_check_bus(char);
extern void pci_register_device(char, char, char);
extern char pci_check_device(char, char, char);

extern void pci_write_config_8(char, char, char, char, uint8_t);
extern void pci_write_config_16(char, char, char, char, uint16_t);
extern void pci_write_config_32(char, char, char, char, uint32_t);

extern uint8_t pci_read_config_8(char, char, char, char);
extern uint16_t pci_read_config_16(char, char, char, char);
extern uint32_t pci_read_config_32(char, char, char, char);

extern void pci_check_bus( char );
extern void pci_check_all_buses();
extern void pci_check_device( char, char );
extern void pci_check_bridge( char, char, char );

extern void pci_get_info_three (
  unsigned char		baseid,
  unsigned char		subid,
  unsigned char		progid,
  char **		basedesc,
  char **		subdesc,
  char **		progdesc
);

void
pci_get_info (
  long		classcode,
  char **	base,
  char **	sub,
  char **	prog
);

extern char *pci_get_ven_name( uint16_t );
extern char *pci_get_dev_type( uint8_t, uint8_t, uint8_t );