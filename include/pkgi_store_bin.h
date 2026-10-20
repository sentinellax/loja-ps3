#pragma once

#include <stdint.h>
#include <stddef.h>

#define STORE_BIN_MAGIC 0x50533353 // "PS3S"
#define STORE_BIN_VERSION 1

#define PLATFORM_PS3 1
#define PLATFORM_PS2 2
#define PLATFORM_PS1 3
#define PLATFORM_PSP 4
#define PLATFORM_EXTRA 5

#define REGION_OTHER 0
#define REGION_USA 1
#define REGION_EUR 2
#define REGION_JPN 3
#define REGION_ASIA 4

#define CAT_GAME 1
#define CAT_DLC 2
#define CAT_THEME 3
#define CAT_AVATAR 4
#define CAT_DEMO 5
#define CAT_UPDATE 6

#pragma pack(push, 1)
typedef struct {
    char title_id[10];     // "BLUS12345\0"
    uint8_t platform;      // 1=PS3, 2=PS2, 3=PS1, 4=PSP
    uint8_t region;        // 1=USA, 2=EUR, 3=JPN, 4=ASIA, 0=OTHER
    uint8_t category;      // 1=Game, 2=DLC, 3=Theme, 4=Avatar, etc.
    uint8_t flags;         // bit 0: rap, bit 1: desc
    uint64_t file_size;    // size in bytes
    uint32_t name_offset;  // offset in strings table
    uint32_t url_offset;   // offset in strings table
    uint32_t rap_offset;   // offset in strings table
    uint32_t cid_offset;   // offset in strings table
    uint32_t desc_offset;  // offset in strings table
} StoreRecord;

typedef struct {
    char magic[4];          // "PS3S"
    uint16_t version;       // 1
    uint32_t item_count;    // total records
    uint32_t records_offset;// byte offset to records
    uint32_t records_size;  // total size of records
    uint32_t strings_offset;// byte offset to strings
    uint32_t strings_size;  // total size of strings
    uint8_t reserved[38];   // padding to 64 bytes
} StoreHeader;
#pragma pack(pop)

// API functions
int store_bin_load(const char* filepath);
void store_bin_free(void);

uint32_t store_bin_get_total(void);
const StoreRecord* store_bin_get_record(uint32_t index);
const char* store_bin_get_string(uint32_t offset);

// Filter & Search functions
uint32_t store_bin_filter(uint8_t platform, uint8_t cat_mask, uint8_t reg_mask, const char* search_query, uint32_t* out_indices, uint32_t max_indices);
