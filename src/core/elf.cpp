// elf.h - ELF file parsing
#include "includes.h"
#include "lib/elf.h"

elf_header_32* read_elf_header( void *file_data ) {
    uint8_t *fdata_8   = (uint8_t*)file_data;
    uint16_t *fdata_16 = (uint16_t*)file_data;
    uint32_t *fdata_32 = (uint32_t*)file_data;
    elf_header_32 *hdr = new elf_header_32;
    
    hdr->signature          = fdata_32[0]; // 0x00 - 0x03
    hdr->bit_width          = fdata_8[4];  // 0x03 - 0x04
    hdr->endianness         = fdata_8[5];  // 0x04 - 0x05
    hdr->elf_version        = fdata_8[6];  // 0x06 - 0x07
    hdr->abi                = fdata_8[7];  // 0x07 - 0x08
    hdr->abi_ver            = fdata_8[8];  // 0x08 - 0x09
    // let's just assume that there are 7 bytes of padding at 0x09 (source: wikipedia)
    hdr->type               = fdata_16[8]; // 0x10 - 0x12
    hdr->arch               = fdata_16[9]; // 0x12 - 0x14
    hdr->version            = fdata_32[5]; // 0x14 - 0x18
    hdr->entry_pt           = fdata_32[6]; // 0x18 - 0x1C
    hdr->ph_offset          = fdata_32[7]; // 0x1C - 0x20
    hdr->sh_offset          = fdata_32[8]; // 0x20 - 0x24
    hdr->flags              = fdata_32[9]; // 0x24 - 0x28
    hdr->header_sz          = fdata_16[20]; // 0x28 - 0x2A
    hdr->ph_entry_sz        = fdata_16[21]; // 0x2A - 0x2C
    hdr->n_ph_entries       = fdata_16[22]; // 0x2C - 0x2E
    hdr->sh_entry_sz        = fdata_16[23]; // 0x2E - 0x30
    hdr->n_sh_entries       = fdata_16[24]; // 0x30 - 0x32
    hdr->string_table_index = fdata_16[25]; // 0x32 - 0x34
    return hdr;
}