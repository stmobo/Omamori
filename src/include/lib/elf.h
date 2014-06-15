#pragma once
#include "includes.h"

typedef struct elf32_header {
    uint32_t      signature;
    uint8_t       bit_width;
    uint8_t       endianness;
    uint8_t       elf_version;
    uint8_t       abi;
    uint8_t       abi_ver;
    uint16_t      type;
    uint16_t      arch;
    uint32_t      version;
    uint32_t      entry_pt;
    uint32_t      ph_offset;
    uint32_t      sh_offset;
    uint32_t      flags;
    uint16_t      header_sz;
    uint16_t      ph_entry_sz;
    uint16_t      n_ph_entries;
    uint16_t      sh_entry_sz;
    uint16_t      n_sh_entries;
    uint16_t      string_table_index;
} elf_header_32;

typedef struct elf64_header {
    uint32_t      signature;
    uint8_t       bit_width;
    uint8_t       endianness;
    uint8_t       elf_version;
    uint8_t       abi;
    uint8_t       abi_ver;
    uint16_t      type;
    uint16_t      arch;
    uint32_t      version;
    uint64_t      entry_pt;
    uint64_t      ph_offset;
    uint64_t      sh_offset;
    uint32_t      flags;
    uint16_t      header_sz;
    uint16_t      ph_entry_sz;
    uint16_t      n_ph_entries;
    uint16_t      sh_entry_sz;
    uint16_t      n_sh_entries;
    uint16_t      string_table_index;
} elf_header_64;