#pragma once

#include <stdint.h>
#include "pkgi_style.h"

#define COVERS_CACHE_SIZE 24

#pragma pack(push, 1)
typedef struct {
    char magic[4];       // "COVR"
    uint16_t version;    // 1
    uint32_t count;      // total covers
    uint8_t reserved[10];
} CoversDatHeader;

typedef struct {
    char title_id[10];
    uint32_t offset;
    uint32_t size;
} CoverDatEntry;
#pragma pack(pop)

int pkgi_covers_init(const char* covers_dat_path);
void pkgi_covers_shutdown(void);

// Returns texture from LRU cache or loads it on-demand from covers.dat.
// Returns 0 / NULL texture if no cover exists.
pkgi_texture pkgi_covers_get(const char* title_id);
