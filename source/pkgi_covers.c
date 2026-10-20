#include "pkgi_covers.h"
#include "pkgi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE* g_covers_file = NULL;
static CoverDatEntry* g_covers_table = NULL;
static uint32_t g_covers_count = 0;

typedef struct {
    char title_id[10];
    pkgi_texture texture;
    uint32_t last_used;
} TextureCacheEntry;

static TextureCacheEntry g_lru_cache[COVERS_CACHE_SIZE];
static uint32_t g_usage_counter = 0;

int pkgi_covers_init(const char* covers_dat_path)
{
    pkgi_covers_shutdown();

    g_covers_file = fopen(covers_dat_path, "rb");
    if (!g_covers_file) {
        return 0;
    }

    CoversDatHeader hdr;
    if (fread(&hdr, 1, sizeof(CoversDatHeader), g_covers_file) != sizeof(CoversDatHeader)) {
        fclose(g_covers_file);
        g_covers_file = NULL;
        return 0;
    }

    if (memcmp(hdr.magic, "COVR", 4) != 0) {
        fclose(g_covers_file);
        g_covers_file = NULL;
        return 0;
    }

    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    g_covers_count = __builtin_bswap32(hdr.count);
    #else
    g_covers_count = hdr.count;
    #endif

    if (g_covers_count == 0) {
        return 1;
    }

    size_t table_sz = g_covers_count * sizeof(CoverDatEntry);
    g_covers_table = (CoverDatEntry*)malloc(table_sz);
    if (!g_covers_table) {
        fclose(g_covers_file);
        g_covers_file = NULL;
        return 0;
    }

    if (fread(g_covers_table, 1, table_sz, g_covers_file) != table_sz) {
        free(g_covers_table);
        g_covers_table = NULL;
        fclose(g_covers_file);
        g_covers_file = NULL;
        return 0;
    }

    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    for (uint32_t i = 0; i < g_covers_count; i++) {
        g_covers_table[i].offset = __builtin_bswap32(g_covers_table[i].offset);
        g_covers_table[i].size = __builtin_bswap32(g_covers_table[i].size);
    }
    #endif

    return 1;
}

void pkgi_covers_shutdown(void)
{
    for (int i = 0; i < COVERS_CACHE_SIZE; i++) {
        if (g_lru_cache[i].texture) {
            pkgi_free_texture(g_lru_cache[i].texture);
            g_lru_cache[i].texture = 0;
        }
        g_lru_cache[i].title_id[0] = '\0';
    }

    if (g_covers_table) {
        free(g_covers_table);
        g_covers_table = NULL;
    }
    if (g_covers_file) {
        fclose(g_covers_file);
        g_covers_file = NULL;
    }
    g_covers_count = 0;
}

static const CoverDatEntry* find_cover_entry(const char* title_id)
{
    if (!g_covers_table || g_covers_count == 0) return NULL;

    int low = 0;
    int high = (int)g_covers_count - 1;

    char target[10] = {0};
    strncpy(target, title_id, 9);

    while (low <= high) {
        int mid = low + (high - low) / 2;
        int cmp = strncmp(target, g_covers_table[mid].title_id, 9);
        if (cmp == 0) {
            return &g_covers_table[mid];
        } else if (cmp < 0) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }
    return NULL;
}

pkgi_texture pkgi_covers_get(const char* title_id)
{
    if (!title_id || !title_id[0]) return 0;

    g_usage_counter++;

    // 1. Check LRU cache
    int free_slot = -1;
    int lru_slot = 0;
    uint32_t oldest_time = 0xFFFFFFFF;

    for (int i = 0; i < COVERS_CACHE_SIZE; i++) {
        if (g_lru_cache[i].texture != 0) {
            if (strncmp(g_lru_cache[i].title_id, title_id, 9) == 0) {
                g_lru_cache[i].last_used = g_usage_counter;
                return g_lru_cache[i].texture;
            }
            if (g_lru_cache[i].last_used < oldest_time) {
                oldest_time = g_lru_cache[i].last_used;
                lru_slot = i;
            }
        } else if (free_slot == -1) {
            free_slot = i;
        }
    }

    // 2. Not in cache, look up in covers.dat
    const CoverDatEntry* entry = find_cover_entry(title_id);
    if (!entry || !g_covers_file || entry->size == 0) {
        return 0; // Return 0 to trigger the modern geometric fallback card!
    }

    uint8_t* blob = (uint8_t*)malloc(entry->size);
    if (!blob) return 0;

    fseek(g_covers_file, entry->offset, SEEK_SET);
    if (fread(blob, 1, entry->size, g_covers_file) != entry->size) {
        free(blob);
        return 0;
    }

    // Load as RSX hardware texture
    pkgi_texture tex = pkgi_load_jpg_raw(blob, entry->size);
    free(blob);

    if (!tex) return 0;

    // 3. Put into cache
    int target_slot = (free_slot != -1) ? free_slot : lru_slot;
    if (g_lru_cache[target_slot].texture != 0) {
        pkgi_free_texture(g_lru_cache[target_slot].texture);
    }

    strncpy(g_lru_cache[target_slot].title_id, title_id, 9);
    g_lru_cache[target_slot].title_id[9] = '\0';
    g_lru_cache[target_slot].texture = tex;
    g_lru_cache[target_slot].last_used = g_usage_counter;

    return tex;
}
