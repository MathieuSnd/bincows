#pragma once

#include <stdint.h>

/*
    stolen from wiki.osdev.org/FAT
*/
typedef struct fat_BS
{
	uint8_t  bootjmp[3];
	uint8_t  oem_name[8];
	uint16_t bytes_per_sector;
	uint8_t	 ectors_per_cluster;
	uint16_t reserved_sector_count;
	uint8_t	 able_count;
	uint16_t root_entry_count;
	uint16_t total_sectors_16;
	uint8_t	 edia_type;
	uint16_t table_size_16;
	uint16_t sectors_per_track;
	uint16_t head_side_count;
	uint32_t hidden_sector_count;
	uint32_t total_sectors_32;
 
	// this will be cast to it's specific type once the driver 
    // actually knows what type of FAT this is.
	uint8_t	extended_section[54];
 
} __attribute__((packed)) fat_BS_t;

#define FAT32_READ_ONLY (0x01)
#define FAT32_HIDDEN (0x02)
#define FAT32_SYSTEM (0x04)
#define FAT32_VOLUME_ID (0x08)
#define FAT32_DIRECTORY (0x10)
#define FAT32_ARCHIVE (0x20)
#define FAT32_LFN (0xf)


typedef struct fat_dir {
	char name[11];
	uint8_t  attr;
	uint8_t  reserved;
	uint8_t  date0;
	uint16_t date1;
	uint16_t date2;
	uint16_t date3;
	uint16_t cluster_high;
	uint16_t date4;
	uint16_t date5;
	uint16_t cluster_low;
	uint32_t file_size;
} __attribute__((packed)) fat_dir_t;


typedef struct fat_long_filename {
	uint8_t  order;
	uint16_t chars0[5];
	uint8_t  attr; // = FAT32_LFN
	uint8_t  checksum;
	uint8_t  type;
	uint16_t chars1[6];
	uint16_t zero;
	uint16_t chars2[2];
} __attribute__((packed)) fat_long_filename_t;


static_assert_equals(sizeof(fat_dir_t), 32);
static_assert_equals(sizeof(fat_long_filename_t), 32);

