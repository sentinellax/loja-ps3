#include "pkgi_ui_modern.h"
#include "pkgi_store_bin.h"
#include "pkgi_covers.h"
#include "pkgi_download.h"
#include "pkgi.h"
#include <stdio.h>
#include <string.h>

// Colors (0xAARRGGBB)
#define COLOR_BG            0xFF0B111E // Deep dark blue / slate
#define COLOR_CARD_BG       0xFF161F30 // Dark card background
#define COLOR_CARD_HOVER    0xFF1E293B // Hover card background
#define COLOR_ACCENT_BLUE   0xFF2563EB // PlayStation blue
#define COLOR_SELECTION     0xFF3B82F6 // Glowing bright blue selection border
#define COLOR_TEXT_WHITE    0xFFF8FAFC
#define COLOR_TEXT_MUTED    0xFF94A3B8
#define COLOR_TEXT_DARK     0xFF64748B
#define COLOR_BORDER        0xFF334155

// Platform badge colors
#define COLOR_BADGE_PS3     0xFF1E3A8A // Dark blue
#define COLOR_BADGE_PS2     0xFF7C2D12 // Dark orange-brown
#define COLOR_BADGE_PS1     0xFF064E3B // Dark green
#define COLOR_BADGE_PSP     0xFF581C87 // Dark purple

// UI State
static StoreView g_current_view = VIEW_HOME;
static StoreTab g_current_tab = TAB_HOME;

static uint32_t g_selected_index = 0; // selected item index in filtered list
static uint32_t g_scroll_offset = 0;   // first visible row/column

static uint32_t g_filtered_indices[16384];
static uint32_t g_filtered_count = 0;

static uint32_t g_detail_record_index = 0;

// Filters
static uint8_t g_filter_cat = 0x02; // Bit 1 = CAT_GAME
static uint8_t g_filter_reg = 0x00; // 0 = all regions
static char g_search_query[64] = "";

static int g_wants_exit = 0;

static const char* TAB_NAMES[TAB_COUNT] = {
    "Início", "PS3", "PS2", "PS1", "PSP", "Apps e temas"
};

static void refresh_filter(void)
{
    uint8_t plat = 0;
    if (g_current_tab == TAB_PS3) plat = PLATFORM_PS3;
    else if (g_current_tab == TAB_PS2) plat = PLATFORM_PS2;
    else if (g_current_tab == TAB_PS1) plat = PLATFORM_PS1;
    else if (g_current_tab == TAB_PSP) plat = PLATFORM_PSP;

    g_filtered_count = store_bin_filter(plat, g_filter_cat, g_filter_reg, g_search_query, g_filtered_indices, 16384);
    if (g_selected_index >= g_filtered_count && g_filtered_count > 0) {
        g_selected_index = g_filtered_count - 1;
    }
}

void ui_modern_init(void)
{
    g_current_view = VIEW_HOME;
    g_current_tab = TAB_HOME;
    g_selected_index = 0;
    g_scroll_offset = 0;
    refresh_filter();
}

int ui_modern_wants_exit(void)
{
    return g_wants_exit;
}

void ui_modern_set_view(StoreView view)
{
    g_current_view = view;
}

void ui_modern_open_details(uint32_t record_index)
{
    g_detail_record_index = record_index;
    g_current_view = VIEW_DETAILS;
}

static void draw_rounded_card(int x, int y, int w, int h, uint32_t bg_color, int selected)
{
    pkgi_draw_fill_rect(x, y, w, h, bg_color);
    if (selected) {
        // Glowing double border for clear controller selection
        pkgi_draw_rect(x - 2, y - 2, w + 4, h + 4, COLOR_SELECTION);
        pkgi_draw_rect(x - 1, y - 1, w + 2, h + 2, COLOR_ACCENT_BLUE);
    } else {
        pkgi_draw_rect(x, y, w, h, COLOR_BORDER);
    }
}

static void draw_header(void)
{
    // App Logo / Title
    pkgi_draw_text_ttf(40, 28, 0, COLOR_TEXT_WHITE, "[LOJA PS3]");

    // Tab Navigation Bar
    int tab_x = 220;
    for (int i = 0; i < TAB_COUNT; i++) {
        int tw = pkgi_text_width_ttf(TAB_NAMES[i]) + 28;
        if (i == (int)g_current_tab) {
            pkgi_draw_fill_rect(tab_x, 24, tw, 36, COLOR_ACCENT_BLUE);
            pkgi_draw_text_ttf(tab_x + 14, 30, 0, COLOR_TEXT_WHITE, TAB_NAMES[i]);
        } else {
            pkgi_draw_text_ttf(tab_x + 14, 30, 0, COLOR_TEXT_MUTED, TAB_NAMES[i]);
        }
        tab_x += tw + 10;
    }

    // Free HDD space indicator on top right
    uint64_t free_bytes = pkgi_get_free_space();
    char free_str[64];
    snprintf(free_str, sizeof(free_str), "Livre: %.1f GB", (double)free_bytes / (1024.0 * 1024.0 * 1024.0));
    int free_w = pkgi_text_width_ttf(free_str);
    pkgi_draw_text_ttf(1880 - free_w, 30, 0, COLOR_TEXT_MUTED, free_str);

    // Separator line
    pkgi_draw_fill_rect(40, 75, 1840, 1, COLOR_BORDER);
}

static void draw_footer(void)
{
    pkgi_draw_fill_rect(40, 1020, 1840, 1, COLOR_BORDER);

    int fx = 40;
    // Legend buttons
    pkgi_draw_text_ttf(fx, 1035, 0, COLOR_TEXT_WHITE, "[X] Baixar");
    fx += 160;
    pkgi_draw_text_ttf(fx, 1035, 0, COLOR_TEXT_WHITE, "[▲] Filtros");
    fx += 160;
    pkgi_draw_text_ttf(fx, 1035, 0, COLOR_TEXT_WHITE, "[■] Detalhes");
    fx += 160;
    pkgi_draw_text_ttf(fx, 1035, 0, COLOR_TEXT_WHITE, "[O] Voltar / Sair");

    pkgi_draw_text_ttf(1700, 1035, 0, COLOR_TEXT_DARK, "Interface Moderna v1.0");
}

static void draw_game_card(int x, int y, int w, int h, uint32_t record_idx, int selected)
{
    const StoreRecord* rec = store_bin_get_record(record_idx);
    if (!rec) return;

    draw_rounded_card(x, y, w, h, COLOR_CARD_BG, selected);

    // Try loading cover texture
    pkgi_texture tex = pkgi_covers_get(rec->title_id);
    if (tex) {
        // Draw texture inside card
        pkgi_draw_texture_z(tex, x + 8, y + 8, 0, 0.95f);
    } else {
        // Modern Minimalist Geometric Placeholder (as in user mockup Image 1 and 3!)
        uint32_t placeholder_color = COLOR_BADGE_PS3;
        if (rec->platform == PLATFORM_PS2) placeholder_color = COLOR_BADGE_PS2;
        else if (rec->platform == PLATFORM_PS1) placeholder_color = COLOR_BADGE_PS1;
        else if (rec->platform == PLATFORM_PSP) placeholder_color = COLOR_BADGE_PSP;

        pkgi_draw_fill_rect(x + 10, y + 10, w - 20, h - 80, placeholder_color);

        // Center geometric emblem
        pkgi_draw_fill_rect(x + (w/2) - 25, y + 80, 50, 50, 0xFFE2B340);
    }

    // Platform Badge top-left of image
    const char* plat_label = "PS3";
    uint32_t badge_bg = COLOR_BADGE_PS3;
    if (rec->platform == PLATFORM_PS2) { plat_label = "PS2"; badge_bg = COLOR_BADGE_PS2; }
    else if (rec->platform == PLATFORM_PS1) { plat_label = "PS1"; badge_bg = COLOR_BADGE_PS1; }
    else if (rec->platform == PLATFORM_PSP) { plat_label = "PSP"; badge_bg = COLOR_BADGE_PSP; }

    pkgi_draw_fill_rect(x + 16, y + 16, 46, 22, badge_bg);
    pkgi_draw_text_ttf(x + 22, y + 18, 0, COLOR_TEXT_WHITE, plat_label);

    // Game Title below image
    const char* title = store_bin_get_string(rec->name_offset);
    char short_title[32];
    strncpy(short_title, title, 28);
    short_title[28] = '\0';
    if (strlen(title) > 28) strcat(short_title, "...");

    pkgi_draw_text_ttf(x + 10, y + h - 55, 0, COLOR_TEXT_WHITE, short_title);

    // Size / Region below title
    char meta[48];
    snprintf(meta, sizeof(meta), "%.1f GB", (double)rec->file_size / (1024.0 * 1024.0 * 1024.0));
    pkgi_draw_text_ttf(x + 10, y + h - 30, 0, COLOR_TEXT_MUTED, meta);
}

// ----------------- RENDER VIEWS -----------------

static void render_view_home(void)
{
    // 1. Featured Banner (Em Destaque)
    int hero_x = 40, hero_y = 100, hero_w = 1100, hero_h = 360;
    draw_rounded_card(hero_x, hero_y, hero_w, hero_h, COLOR_CARD_BG, 0);

    if (g_filtered_count > 0) {
        uint32_t feat_idx = g_filtered_indices[0];
        const StoreRecord* rec = store_bin_get_record(feat_idx);
        if (rec) {
            // Badges
            pkgi_draw_fill_rect(hero_x + 30, hero_y + 30, 48, 24, COLOR_BADGE_PS3);
            pkgi_draw_text_ttf(hero_x + 38, hero_y + 34, 0, COLOR_TEXT_WHITE, "PS3");

            pkgi_draw_fill_rect(hero_x + 90, hero_y + 30, 70, 24, 0xFF334155);
            pkgi_draw_text_ttf(hero_x + 100, hero_y + 34, 0, COLOR_TEXT_WHITE, "Jogo");

            // Title
            const char* title = store_bin_get_string(rec->name_offset);
            pkgi_draw_text_ttf(hero_x + 30, hero_y + 80, 0, COLOR_TEXT_WHITE, title);

            // Description
            pkgi_draw_text_ttf(hero_x + 30, hero_y + 140, 0, COLOR_TEXT_MUTED, "Pressione [X] para baixar direto ou [■] para ver detalhes completos.");

            // Buttons
            pkgi_draw_fill_rect(hero_x + 30, hero_y + 260, 160, 48, COLOR_ACCENT_BLUE);
            pkgi_draw_text_ttf(hero_x + 60, hero_y + 274, 0, COLOR_TEXT_WHITE, "Baixar");

            pkgi_draw_fill_rect(hero_x + 210, hero_y + 260, 160, 48, 0xFF1E293B);
            pkgi_draw_text_ttf(hero_x + 240, hero_y + 274, 0, COLOR_TEXT_WHITE, "Detalhes");
        }
    }

    // 2. Sidebar Highlights (Direita)
    int side_x = 1170, side_y = 100, side_w = 710, side_h = 360;
    draw_rounded_card(side_x, side_y, side_w, side_h, COLOR_CARD_BG, 0);
    pkgi_draw_text_ttf(side_x + 30, side_y + 25, 0, COLOR_TEXT_WHITE, "Em destaque");

    // 3. Grid of Novidades (Abaixo)
    pkgi_draw_text_ttf(40, 490, 0, COLOR_TEXT_WHITE, "Novidades");
    pkgi_draw_text_ttf(1800, 490, 0, COLOR_ACCENT_BLUE, "Ver tudo");

    int card_w = 210, card_h = 320, gap = 20;
    int grid_x = 40, grid_y = 530;

    for (int col = 0; col < 8; col++) {
        uint32_t item_pos = g_scroll_offset + col;
        if (item_pos >= g_filtered_count) break;

        int selected = (item_pos == g_selected_index);
        draw_game_card(grid_x + col * (card_w + gap), grid_y, card_w, card_h, g_filtered_indices[item_pos], selected);
    }
}

static void render_view_catalog(void)
{
    // Left Filter Drawer (Width: 360)
    int drawer_x = 40, drawer_y = 100, drawer_w = 340, drawer_h = 900;
    draw_rounded_card(drawer_x, drawer_y, drawer_w, drawer_h, COLOR_CARD_BG, 0);

    pkgi_draw_text_ttf(drawer_x + 20, drawer_y + 20, 0, COLOR_TEXT_WHITE, "Filtros");

    int fy = drawer_y + 70;
    pkgi_draw_text_ttf(drawer_x + 20, fy, 0, COLOR_TEXT_MUTED, "TIPO");
    fy += 35;
    pkgi_draw_text_ttf(drawer_x + 20, fy, 0, COLOR_TEXT_WHITE, "[X] Jogos");
    fy += 35;
    pkgi_draw_text_ttf(drawer_x + 20, fy, 0, COLOR_TEXT_WHITE, "[X] DLCs");
    fy += 35;
    pkgi_draw_text_ttf(drawer_x + 20, fy, 0, COLOR_TEXT_WHITE, "[ ] Demos");

    fy += 60;
    pkgi_draw_text_ttf(drawer_x + 20, fy, 0, COLOR_TEXT_MUTED, "REGIÃO");
    fy += 35;
    pkgi_draw_fill_rect(drawer_x + 20, fy, 60, 30, COLOR_ACCENT_BLUE);
    pkgi_draw_text_ttf(drawer_x + 30, fy + 6, 0, COLOR_TEXT_WHITE, "USA");
    pkgi_draw_fill_rect(drawer_x + 90, fy, 60, 30, 0xFF1E293B);
    pkgi_draw_text_ttf(drawer_x + 100, fy + 6, 0, COLOR_TEXT_WHITE, "EUR");
    pkgi_draw_fill_rect(drawer_x + 160, fy, 60, 30, 0xFF1E293B);
    pkgi_draw_text_ttf(drawer_x + 170, fy + 6, 0, COLOR_TEXT_WHITE, "JPN");

    // Right Content Grid (2 rows x 6 cols)
    int grid_x = 410, grid_y = 100;
    int card_w = 220, card_h = 320, gap_x = 22, gap_y = 25;

    // Search bar at top of grid
    pkgi_draw_fill_rect(grid_x, grid_y, 800, 42, COLOR_CARD_BG);
    pkgi_draw_rect(grid_x, grid_y, 800, 42, COLOR_BORDER);
    pkgi_draw_text_ttf(grid_x + 15, grid_y + 10, 0, COLOR_TEXT_MUTED, g_search_query[0] ? g_search_query : "Buscar por nome (Pressione Select)...");

    char count_str[64];
    snprintf(count_str, sizeof(count_str), "%u itens após os filtros", g_filtered_count);
    pkgi_draw_text_ttf(1600, grid_y + 10, 0, COLOR_TEXT_MUTED, count_str);

    grid_y += 65;

    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 6; col++) {
            uint32_t item_pos = g_scroll_offset + (row * 6) + col;
            if (item_pos >= g_filtered_count) break;

            int selected = (item_pos == g_selected_index);
            draw_game_card(grid_x + col * (card_w + gap_x), grid_y + row * (card_h + gap_y), card_w, card_h, g_filtered_indices[item_pos], selected);
        }
    }
}

static void render_view_details(void)
{
    const StoreRecord* rec = store_bin_get_record(g_detail_record_index);
    if (!rec) return;

    // Back button
    pkgi_draw_text_ttf(60, 100, 0, COLOR_TEXT_MUTED, "< Voltar para Catálogo");

    // Cover on left
    int cover_x = 60, cover_y = 150, cover_w = 340, cover_h = 480;
    draw_rounded_card(cover_x, cover_y, cover_w, cover_h, COLOR_CARD_BG, 0);
    pkgi_texture tex = pkgi_covers_get(rec->title_id);
    if (tex) {
        pkgi_draw_texture_z(tex, cover_x + 15, cover_y + 15, 0, 1.4f);
    }

    // Details on right
    int info_x = 440, info_y = 150;

    // Title
    const char* title = store_bin_get_string(rec->name_offset);
    pkgi_draw_text_ttf(info_x, info_y + 20, 0, COLOR_TEXT_WHITE, title);

    // Tags
    pkgi_draw_fill_rect(info_x, info_y + 80, 80, 28, COLOR_ACCENT_BLUE);
    pkgi_draw_text_ttf(info_x + 15, info_y + 84, 0, COLOR_TEXT_WHITE, "Jogo");

    // Spec cards
    int spec_y = info_y + 160;
    int spec_w = 180, spec_h = 80;

    // Tamanho
    draw_rounded_card(info_x, spec_y, spec_w, spec_h, COLOR_CARD_BG, 0);
    pkgi_draw_text_ttf(info_x + 15, spec_y + 12, 0, COLOR_TEXT_MUTED, "TAMANHO");
    char sz_str[32];
    snprintf(sz_str, sizeof(sz_str), "%.1f GB", (double)rec->file_size / (1024.0 * 1024.0 * 1024.0));
    pkgi_draw_text_ttf(info_x + 15, spec_y + 40, 0, COLOR_TEXT_WHITE, sz_str);

    // Região
    draw_rounded_card(info_x + 200, spec_y, spec_w, spec_h, COLOR_CARD_BG, 0);
    pkgi_draw_text_ttf(info_x + 215, spec_y + 12, 0, COLOR_TEXT_MUTED, "REGIÃO");
    pkgi_draw_text_ttf(info_x + 215, spec_y + 40, 0, COLOR_TEXT_WHITE, "USA");

    // Plataforma
    draw_rounded_card(info_x + 400, spec_y, spec_w, spec_h, COLOR_CARD_BG, 0);
    pkgi_draw_text_ttf(info_x + 415, spec_y + 12, 0, COLOR_TEXT_MUTED, "PLATAFORMA");
    pkgi_draw_text_ttf(info_x + 415, spec_y + 40, 0, COLOR_TEXT_WHITE, "PS3");

    // Action buttons
    int btn_y = spec_y + 130;
    pkgi_draw_fill_rect(info_x, btn_y, 220, 52, COLOR_ACCENT_BLUE);
    pkgi_draw_text_ttf(info_x + 35, btn_y + 16, 0, COLOR_TEXT_WHITE, "Baixar e instalar");

    pkgi_draw_fill_rect(info_x + 250, btn_y, 160, 52, 0xFF1E293B);
    pkgi_draw_text_ttf(info_x + 285, btn_y + 16, 0, COLOR_TEXT_WHITE, "Só baixar");
}

void ui_modern_render(void)
{
    // Draw solid dark background
    pkgi_draw_fill_rect(0, 0, 1920, 1080, COLOR_BG);

    draw_header();

    switch (g_current_view) {
        case VIEW_HOME:
            render_view_home();
            break;
        case VIEW_CATALOG:
        case VIEW_FILTER_MENU:
            render_view_catalog();
            break;
        case VIEW_DETAILS:
            render_view_details();
            break;
    }

    draw_footer();
}

void ui_modern_update(uint32_t buttons_down, uint32_t buttons_pressed)
{
    // L1 / R1: Change Tabs
    if (buttons_pressed & PKGI_BUTTON_L1) {
        if (g_current_tab > 0) {
            g_current_tab--;
            g_current_view = (g_current_tab == TAB_HOME) ? VIEW_HOME : VIEW_CATALOG;
            refresh_filter();
        }
    } else if (buttons_pressed & PKGI_BUTTON_R1) {
        if (g_current_tab < TAB_COUNT - 1) {
            g_current_tab++;
            g_current_view = (g_current_tab == TAB_HOME) ? VIEW_HOME : VIEW_CATALOG;
            refresh_filter();
        }
    }

    // Navigation in Grid (D-Pad / Analógico)
    if (g_current_view == VIEW_HOME || g_current_view == VIEW_CATALOG) {
        int cols = (g_current_view == VIEW_HOME) ? 8 : 6;

        if (buttons_pressed & PKGI_BUTTON_RIGHT) {
            if (g_selected_index + 1 < g_filtered_count) {
                g_selected_index++;
            }
        } else if (buttons_pressed & PKGI_BUTTON_LEFT) {
            if (g_selected_index > 0) {
                g_selected_index--;
            }
        } else if (buttons_pressed & PKGI_BUTTON_DOWN) {
            if (g_selected_index + cols < g_filtered_count) {
                g_selected_index += cols;
            }
        } else if (buttons_pressed & PKGI_BUTTON_UP) {
            if (g_selected_index >= (uint32_t)cols) {
                g_selected_index -= cols;
            }
        }

        // Adjust scroll offset so selected item is always visible
        if (g_selected_index < g_scroll_offset) {
            g_scroll_offset = g_selected_index;
        } else if (g_selected_index >= g_scroll_offset + (cols * 2)) {
            g_scroll_offset = g_selected_index - (cols * 2) + 1;
        }

        // Cross (X) / Square ([]): Open Details
        if ((buttons_pressed & PKGI_BUTTON_X) || (buttons_pressed & PKGI_BUTTON_SQUARE)) {
            if (g_filtered_count > 0) {
                ui_modern_open_details(g_filtered_indices[g_selected_index]);
            }
        }

        // Triangle (/\): Toggle view between Home and Catalog
        if (buttons_pressed & PKGI_BUTTON_TRIANGLE) {
            g_current_view = (g_current_view == VIEW_HOME) ? VIEW_CATALOG : VIEW_HOME;
        }

        // Circle (O): Exit
        if (buttons_pressed & PKGI_BUTTON_O) {
            g_wants_exit = 1;
        }
    } else if (g_current_view == VIEW_DETAILS) {
        // Circle (O): Back to catalog
        if (buttons_pressed & PKGI_BUTTON_O) {
            g_current_view = (g_current_tab == TAB_HOME) ? VIEW_HOME : VIEW_CATALOG;
        }

        // Cross (X): Start download
        if (buttons_pressed & PKGI_BUTTON_X) {
            const StoreRecord* rec = store_bin_get_record(g_detail_record_index);
            if (rec) {
                const char* url = store_bin_get_string(rec->url_offset);
                const char* name = store_bin_get_string(rec->name_offset);
                const char* rap = store_bin_get_string(rec->rap_offset);
                // Call standard pkgi download queue!
                pkgi_download_start(name, url, (const uint8_t*)rap, NULL);
            }
        }
    }
}
