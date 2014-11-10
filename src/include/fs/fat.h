// fat.h

class fat32_fs {
    private:
    uint8_t sector_one[512];
    
    public:
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t n_reserved_sectors;
    uint16_t n_file_alloc_tables;
    uint16_t n_directory_entries;
    uint32_t n_sectors;
    uint32_t n_hidden_sectors;
    
    fat32_fs(unsigned int);
}