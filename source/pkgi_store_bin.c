#include "pkgi_store_bin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static uint8_t* g_store_data = NULL;
static size_t g_store_data_size = 0;

static StoreHeader g_header;
static const StoreRecord* g_records = NULL;
static const char* g_strings = NULL;

int store_bin_load(const char* filepath)
{
    store_bin_free();

    FILE* f = fopen(filepath, "rb");
    if (!f) {
        return 0;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < (long)sizeof(StoreHeader)) {
        fclose(f);
        return 0;
    }

    g_store_data = (uint8_t*)malloc(size);
    if (!g_store_data) {
        fclose(f);
        return 0;
    }

    if (fread(g_store_data, 1, size, f) != (size_t)size) {
        free(g_store_data);
        g_store_data = NULL;
        fclose(f);
        return 0;
    }
    fclose(f);

    g_store_data_size = size;

    // Read header (big-endian fields from file)
    memcpy(&g_header, g_store_data, sizeof(StoreHeader));

    if (memcmp(g_header.magic, "PS3S", 4) != 0) {
        store_bin_free();
        return 0;
    }

    // Convert big-endian header fields to host
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define SWAP16(x) __builtin_bswap16(x)
    #define SWAP32(x) __builtin_bswap32(x)
    #else
    #define SWAP16(x) (x)
    #define SWAP32(x) (x)
    #endif

    g_header.version = SWAP16(g_header.version);
    g_header.item_count = SWAP32(g_header.item_count);
    g_header.records_offset = SWAP32(g_header.records_offset);
    g_header.records_size = SWAP32(g_header.records_size);
    g_header.strings_offset = SWAP32(g_header.strings_offset);
    g_header.strings_size = SWAP32(g_header.strings_size);

    if (g_header.records_offset + g_header.records_size > g_store_data_size ||
        g_header.strings_offset + g_header.strings_size > g_store_data_size) {
        store_bin_free();
        return 0;
    }

    g_records = (const StoreRecord*)(g_store_data + g_header.records_offset);
    g_strings = (const char*)(g_store_data + g_header.strings_offset);

    return 1;
}

void store_bin_free(void)
{
    if (g_store_data) {
        free(g_store_data);
        g_store_data = NULL;
    }
    g_records = NULL;
    g_strings = NULL;
    memset(&g_header, 0, sizeof(g_header));
}

uint32_t store_bin_get_total(void)
{
    return g_header.item_count;
}

const StoreRecord* store_bin_get_record(uint32_t index)
{
    if (!g_records || index >= g_header.item_count) {
        return NULL;
    }
    return &g_records[index];
}

const char* store_bin_get_string(uint32_t offset)
{
    if (!g_strings || offset >= g_header.strings_size) {
        return "";
    }
    return g_strings + offset;
}

static int str_contains_nocase(const char* haystack, const char* needle)
{
    if (!needle || !needle[0]) return 1;
    if (!haystack) return 0;

    char h_lower[256];
    char n_lower[256];

    size_t hlen = strlen(haystack);
    size_t nlen = strlen(needle);
    if (hlen >= sizeof(h_lower)) hlen = sizeof(h_lower) - 1;
    if (nlen >= sizeof(n_lower)) nlen = sizeof(n_lower) - 1;

    for (size_t i = 0; i < hlen; i++) h_lower[i] = tolower((unsigned char)haystack[i]);
    h_lower[hlen] = '\0';
    for (size_t i = 0; i < nlen; i++) n_lower[i] = tolower((unsigned char)needle[i]);
    n_lower[nlen] = '\0';

    return strstr(h_lower, n_lower) != NULL;
}

uint32_t store_bin_filter(uint8_t platform, uint8_t cat_mask, uint8_t reg_mask, const char* search_query, uint32_t* out_indices, uint32_t max_indices)
{
    if (!g_records || !out_indices || max_indices == 0) {
        return 0;
    }

    uint32_t count = 0;
    uint32_t total = g_header.item_count;

    for (uint32_t i = 0; i < total; i++) {
        const StoreRecord* rec = &g_records[i];

        // Platform filter (0 = all platforms)
        if (platform != 0 && rec->platform != platform) {
            continue;
        }

        // Category filter bitmask (1 << category)
        if (cat_mask != 0 && !(cat_mask & (1 << rec->category))) {
            continue;
        }

        // Region filter bitmask (1 << region)
        if (reg_mask != 0 && !(reg_mask & (1 << rec->region))) {
            continue;
        }

        // Search text filter
        if (search_query && search_query[0]) {
            #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
            uint32_t name_off = __builtin_bswap32(rec->name_offset);
            #else
            uint32_t name_off = rec->name_offset;
            #endif
            const char* name = store_bin_get_string(name_off);
            if (!str_contains_nocase(name, search_query) && !str_contains_nocase(rec->title_id, search_query)) {
                continue;
            }
        }

        out_indices[count++] = i;
        if (count >= max_indices) {
            break;
        }
    }

    return count;
}
