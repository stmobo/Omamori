// pci.h
#pragma once

#define PCI_IO_CONFIG_ADDRESS       0xCF8
#define PCI_IO_CONFIG_DATA          0xCFC


struct pci_device {
    uint8_t bus;
    uint8_t device;
    uint8_t func;
    uint16_t vendorID;
    uint16_t deviceID;
    uint8_t class_code;
    uint8_t subclass_code;
    uint8_t header_type;
};

typedef struct pci_device pci_device_t;

extern void pci_check_bus(char);
extern void pci_register_device(char, char, char);
extern char pci_check_device(char, char, char);

extern void pci_write_config_8(char, char, char, char, uint8_t);
extern void pci_write_config_16(char, char, char, char, uint16_t);
extern void pci_write_config_32(char, char, char, char, uint32_t);

extern uint8_t pci_read_config_8(char, char, char, char);
extern uint16_t pci_read_config_16(char, char, char, char);
extern uint32_t pci_read_config_32(char, char, char, char);
