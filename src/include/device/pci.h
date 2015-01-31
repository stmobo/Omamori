// pci.h
#pragma once
#include "includes.h"
#include "lib/vector.h"
#include "core/device_manager.h"

#define PCI_IO_CONFIG_ADDRESS       0xCF8
#define PCI_IO_CONFIG_DATA          0xCFC

typedef struct pcie_ecs_range {
    uint32_t mmio_paddr;
    char     bus_start;
    char     bus_end;
    size_t   vaddr;
} pcie_ecs_range;

typedef struct pci_bar {
    uint32_t raw;
    uint32_t phys_address;
    uint32_t virt_address;
    bool io_space;
    char loc_area;
    bool cacheable;
    uint32_t size;
    
    void writeb( uint32_t offset, uint8_t data );
    void writew( uint32_t offset, uint16_t data );
    void writed( uint32_t offset, uint32_t data );
    
    uint8_t  readb( uint32_t offset );
    uint16_t readw( uint32_t offset );
    uint32_t readd( uint32_t offset );
    
    void allocate_mmio();
} pci_bar;

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
    pci_bar registers[6];
    uint8_t secondary_bus;
} pci_device;

extern vector<pci_device*> pci_devices;

extern void pci_check_bus(uint8_t);
extern void pci_register_device(uint8_t, uint8_t, uint8_t);
extern uint8_t pci_check_device(uint8_t, uint8_t, uint8_t);

extern void pci_write_config_8(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);
extern void pci_write_config_16(uint8_t, uint8_t, uint8_t, uint8_t, uint16_t);
extern void pci_write_config_32(uint8_t, uint8_t, uint8_t, uint8_t, uint32_t);

extern uint8_t pci_read_config_8(uint8_t, uint8_t, uint8_t, uint8_t);
extern uint16_t pci_read_config_16(uint8_t, uint8_t, uint8_t, uint8_t);
extern uint32_t pci_read_config_32(uint8_t, uint8_t, uint8_t, uint8_t);

extern void pci_check_bus( uint8_t bus, uint8_t bus_bloc, uint8_t bus_dloc, uint8_t bus_floc );
extern void pci_check_all_buses();
extern void pci_check_device( uint8_t bus, uint8_t device, device_manager::device_node* bus_node );
extern void pci_check_bridge( uint8_t bus, uint8_t device, uint8_t func );
extern void pci_initialize();

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
